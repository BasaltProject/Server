package org.basaltmc.server.runtime.config

import org.basaltmc.server.runtime.command.testGroups
import org.basaltmc.server.runtime.placeholder.PlaceholderSlots
import org.basaltmc.server.runtime.placeholder.PlayerPlaceholders
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The profiles a test server advertises: enough to answer a ping, with no file behind it. */
internal fun testServerList(): ServerListConfig {
    val default =
        ServerListProfile(
            ProfileHeader("default", priority = 0, isDefault = true, isActive = false, conditions = emptyList()),
            motd = listOf("A Basalt Server"),
        )
    return ServerListConfig(listOf(default), default, rememberIp = false)
}

/** Only the player side: nothing here asks the server about itself. */
internal fun testPlaceholders(): PlaceholderRegistry =
    PlaceholderRegistry().apply { register("basalt_player", PlayerPlaceholders({ testGroups() })) }

class ServerListConfigTest {
    private val placeholders = testPlaceholders()

    private fun directoryFor(name: String): ConfigDirectory =
        ConfigDirectory("build/test-data/config_$name", Logger("Test"))

    private fun load(
        name: String,
        contents: String,
    ): ServerListConfig {
        val directory = directoryFor(name)
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(ServerListConfig.FILE_NAME), contents)
        return ServerListConfig.load(directory, logger = Logger("Test"))
    }

    private fun ping(
        protocol: Int = 776,
        host: String = "example.com",
        address: String? = "203.0.113.7",
        knownPlayer: String? = null,
    ): PlaceholderContext =
        PlaceholderContext.EMPTY.with(
            PlaceholderSlots.PING,
            PingRequest(protocol, host, address, knownPlayer),
        )

    private fun ServerListConfig.selected(context: PlaceholderContext) = select(placeholders, context, Logger("Test"))

    @Test
    fun theGeneratedFileGreetsModernClientsWithAGradientAndOlderOnesWithoutOne() {
        val config = load("generated_default", SERVER_LIST_DEFAULT_TOML)

        // 735 is 1.16, where RGB text arrived. Below it a gradient degrades to flat white, which is
        // the entire reason the shipped file has two profiles rather than one.
        assertEquals("default", config.selected(ping(protocol = 776)).name)
        assertEquals("legacy", config.selected(ping(protocol = 734)).name)
        assertTrue(
            config
                .selected(ping(protocol = 776))
                .motd!!
                .first()
                .contains("<gradient:"),
        )
        assertFalse(
            config
                .selected(ping(protocol = 47))
                .motd!!
                .first()
                .contains("<gradient:"),
        )
    }

    @Test
    fun anActiveProfileWinsOverEveryConditionalOne() {
        val config =
            load(
                "active",
                """
                [profiles.default]
                default = true
                motd = "normal"

                [profiles.maintenance]
                active = true
                priority = 100
                motd = "closed"

                [profiles.beta]
                priority = 50
                motd = "beta"
                when = ["%basalt_player_host% = beta.example.com"]
                """.trimIndent(),
            )

        assertEquals("maintenance", config.selected(ping(host = "beta.example.com")).name)
    }

    @Test
    fun thePriorityDecidesBetweenTwoProfilesThatBothApply() {
        val config =
            load(
                "priority",
                """
                [profiles.default]
                default = true
                motd = "normal"

                [profiles.low]
                priority = 1
                motd = "low"
                when = ["%basalt_player_protocol% = 776"]

                [profiles.high]
                priority = 2
                motd = "high"
                when = ["%basalt_player_host% matches *.example.com"]
                """.trimIndent(),
            )

        assertEquals("high", config.selected(ping(host = "mc.example.com")).name)
        assertEquals("low", config.selected(ping(host = "other.test")).name)
    }

    @Test
    fun everyConditionOfAProfileHasToHold() {
        val config =
            load(
                "several",
                """
                [profiles.default]
                default = true
                motd = "normal"

                [profiles.both]
                motd = "both"
                when = [
                    "%basalt_player_protocol% >= 735",
                    "%basalt_player_host% matches *.example.com",
                ]
                """.trimIndent(),
            )

        assertEquals("both", config.selected(ping(protocol = 776, host = "mc.example.com")).name)
        assertEquals("default", config.selected(ping(protocol = 47, host = "mc.example.com")).name)
        assertEquals("default", config.selected(ping(protocol = 776, host = "other.test")).name)
    }

    @Test
    fun aConditionThisSituationCannotAnswerSkipsTheProfileInsteadOfMatchingIt() {
        val config =
            load(
                "unanswerable",
                """
                [profiles.default]
                default = true
                motd = "normal"

                [profiles.impossible]
                priority = 99
                motd = "impossible"
                # A player's coordinates, asked before anyone has logged in.
                when = ["%basalt_player_x% > 100"]

                [profiles.nonsense]
                priority = 98
                motd = "nonsense"
                when = ["%basalt_nope_at_all% = 1"]
                """.trimIndent(),
            )

        assertEquals("default", config.selected(ping()).name)
    }

    @Test
    fun aProfileTakesWhatItDoesNotDeclareFromTheDefaultOne() {
        val config =
            load(
                "inherit",
                """
                [profiles.default]
                default = true
                motd = "normal"
                icon = "custom.png"
                max-players = 100

                [profiles.maintenance]
                active = true
                motd = "closed"
                """.trimIndent(),
            )

        val selected = config.selected(ping())
        assertEquals(listOf("closed"), selected.motd)
        assertEquals("custom.png", selected.icon)
        assertEquals(100, selected.maxPlayers)
    }

    @Test
    fun aMotdWrittenAsOneStringSplitsIntoTheSameTwoLinesAsAList() {
        val config = load("motd_string", "[profiles.default]\ndefault = true\nmotd = \"first\\nsecond\"\n")

        assertEquals(listOf("first", "second"), config.selected(ping()).motd)
    }

    @Test
    fun theHostConditionMatchesTheAddressTheClientConnectedTo() {
        val config =
            load(
                "host",
                """
                [profiles.default]
                default = true
                motd = "normal"

                [profiles.beta]
                motd = "beta"
                when = ["%basalt_player_host% matches beta.*"]
                """.trimIndent(),
            )

        assertEquals("beta", config.selected(ping(host = "beta.example.com")).name)
        assertEquals("beta", config.selected(ping(host = "BETA.example.com")).name)
        assertEquals("default", config.selected(ping(host = "mc.example.com")).name)
    }

    @Test
    fun aConditionWithNoOperatorIsTrueWhenItReadsTrue() {
        val config =
            load(
                "known",
                """
                [profiles.default]
                default = true
                motd = "hello"

                [profiles.returning]
                motd = "welcome back, %basalt_player_name%"
                when = ["%basalt_player_known%"]
                """.trimIndent(),
            )

        assertEquals("returning", config.selected(ping(knownPlayer = "Whiron")).name)
        assertEquals("default", config.selected(ping(knownPlayer = null)).name)
    }

    @Test
    fun rememberingAddressesIsOffUnlessTheFileTurnsItOn() {
        assertFalse(load("ip_default", SERVER_LIST_DEFAULT_TOML).rememberIp)
        assertTrue(
            load(
                "ip_on",
                "[profiles.default]\ndefault = true\nmotd = \"x\"\n\n[greeting]\nremember-ip = true\n",
            ).rememberIp,
        )
    }

    @Test
    fun aFileWithoutExactlyOneDefaultProfileStopsTheServer() {
        assertFailsWith<TomlFormatException> {
            load("no_default", "[profiles.a]\nmotd = \"a\"\n")
        }
        assertFailsWith<TomlFormatException> {
            load("two_defaults", "[profiles.a]\ndefault = true\n\n[profiles.b]\ndefault = true\n")
        }
    }

    @Test
    fun theImportedMotdFromServerPropertiesReplacesBothGreetings() {
        val generated = defaultContents("Imported \"quoted\" MOTD\\nsecond line", Logger("Test"))

        assertFalse(generated.contains("Welcome to your new server"))
        // The maintenance profile keeps its own text: it is not a greeting.
        assertTrue(generated.contains("The server is under maintenance"))
        assertTrue(generated.contains("""    "Imported \"quoted\" MOTD","""))
        assertTrue(generated.contains("""    "second line","""))
        val config = load("imported", generated)
        assertEquals(config.selected(ping(protocol = 776)).motd, config.selected(ping(protocol = 47)).motd)
    }

    @Test
    fun aProfileWithNoConditionWaitsToBeSwitchedOnByHand() {
        // Not an error and not worth warning about: it is how the shipped maintenance profile is
        // written, and it does nothing until `active` turns it on.
        val contents = "[profiles.default]\ndefault = true\nmotd = \"a\"\n\n[profiles.orphan]\nmotd = \"b\"\n"
        val config = load("unreachable", contents)

        assertEquals("default", config.selected(ping()).name)
        assertTrue(
            config.profiles
                .first { it.name == "orphan" }
                .conditions
                .isEmpty(),
        )
    }
}
