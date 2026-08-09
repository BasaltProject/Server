package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The profiles a test server advertises: enough to answer a ping, with no file behind it. */
internal fun testServerList(): ServerListConfig {
    val default = ServerListProfile(name = "default", isDefault = true, motd = listOf("A Basalt Server"))
    return ServerListConfig(listOf(default), default, rememberIp = false)
}

class ServerListConfigTest {
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
    ) = PingRequest(protocol, host, address, knownPlayer)

    @Test
    fun theGeneratedFileGreetsModernClientsWithAGradientAndOlderOnesWithoutOne() {
        val config = load("generated_default", SERVER_LIST_DEFAULT_TOML)

        // 735 is 1.16, where RGB text arrived. Below it a gradient degrades to flat white, which is
        // the entire reason the shipped file has two profiles rather than one.
        assertEquals("default", config.select(ping(protocol = 776)).name)
        assertEquals("legacy", config.select(ping(protocol = 734)).name)
        assertTrue(
            config
                .select(ping(protocol = 776))
                .motd!!
                .first()
                .contains("<gradient:"),
        )
        assertFalse(
            config
                .select(ping(protocol = 47))
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
                [profiles.beta.when]
                host = "beta.example.com"
                """.trimIndent(),
            )

        assertEquals("maintenance", config.select(ping(host = "beta.example.com")).name)
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
                [profiles.low.when]
                protocol = 776

                [profiles.high]
                priority = 2
                motd = "high"
                [profiles.high.when]
                host = "*.example.com"
                """.trimIndent(),
            )

        assertEquals("high", config.select(ping(host = "mc.example.com")).name)
        assertEquals("low", config.select(ping(host = "other.test")).name)
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

        val selected = config.select(ping())
        assertEquals(listOf("closed"), selected.motd)
        assertEquals("custom.png", selected.icon)
        assertEquals(100, selected.maxPlayers)
    }

    @Test
    fun aMotdWrittenAsOneStringSplitsIntoTheSameTwoLinesAsAList() {
        val config = load("motd_string", "[profiles.default]\ndefault = true\nmotd = \"first\\nsecond\"\n")

        assertEquals(listOf("first", "second"), config.select(ping()).motd)
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
                [profiles.beta.when]
                host = "beta.*"
                """.trimIndent(),
            )

        assertEquals("beta", config.select(ping(host = "beta.example.com")).name)
        assertEquals("beta", config.select(ping(host = "BETA.example.com")).name)
        assertEquals("default", config.select(ping(host = "mc.example.com")).name)
    }

    @Test
    fun aKnownPlayerConditionNeedsSomebodyToHaveBeenRecognised() {
        val config =
            load(
                "known",
                """
                [profiles.default]
                default = true
                motd = "hello"

                [profiles.returning]
                motd = "welcome back, <player>"
                [profiles.returning.when]
                known-player = true
                """.trimIndent(),
            )

        assertEquals("returning", config.select(ping(knownPlayer = "Whiron")).name)
        assertEquals("default", config.select(ping(knownPlayer = null)).name)
    }

    @Test
    fun lookingPlayersUpIsSkippedWhenNoProfileAsksForIt() {
        val greeting = "\n[greeting]\nremember-ip = true\n"
        val plain = "[profiles.default]\ndefault = true\nmotd = \"hello\"\n"
        val greeter = "[profiles.default]\ndefault = true\nmotd = \"hello <player>\"\n"

        assertFalse(load("lookup_off", plain + greeting).usesPlayerLookup)
        assertTrue(load("lookup_on", greeter + greeting).usesPlayerLookup)
        // The greeting being off is enough on its own: there is nothing to look up.
        assertFalse(load("lookup_no_ip", greeter).usesPlayerLookup)
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
    fun afileWithoutExactlyOneDefaultProfileStopsTheServer() {
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
        assertTrue(generated.contains("Back shortly"))
        assertTrue(generated.contains("""    "Imported \"quoted\" MOTD","""))
        assertTrue(generated.contains("""    "second line","""))
        // Still a valid file, and both profiles ended up with the same text.
        val config = load("imported", generated)
        assertEquals(config.select(ping(protocol = 776)).motd, config.select(ping(protocol = 47)).motd)
    }

    @Test
    fun globsMatchTheWayTheFileSaysTheyDo() {
        assertTrue(globMatches("*", "anything"))
        assertTrue(globMatches("mc.example.com", "MC.example.com"))
        assertTrue(globMatches("*.example.com", "beta.example.com"))
        assertTrue(globMatches("192.168.*", "192.168.1.4"))
        assertFalse(globMatches("192.168.*", "10.0.0.1"))
        // "a*bc" must not satisfy itself twice out of the same characters.
        assertFalse(globMatches("a*bc", "abc".dropLast(1)))
        assertTrue(globMatches("a*bc", "abc"))
    }

    @Test
    fun aProfileWithNoConditionWaitsToBeSwitchedOnByHand() {
        // Not an error and not worth warning about: it is how the shipped maintenance profile is
        // written, and it does nothing until `active` turns it on.
        val contents = "[profiles.default]\ndefault = true\nmotd = \"a\"\n\n[profiles.orphan]\nmotd = \"b\"\n"
        val config = load("unreachable", contents)

        assertEquals("default", config.select(ping()).name)
        assertNull(
            config.profiles
                .first { it.name == "orphan" }
                .condition.host,
        )
    }
}
