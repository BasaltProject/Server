package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.config.ProfileHeader
import org.basaltmc.server.runtime.config.ServerListConfig
import org.basaltmc.server.runtime.config.ServerListProfile
import org.basaltmc.server.runtime.config.testPlaceholders
import org.basaltmc.wire.common.placeholder.PlaceholderCondition
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ServerListStatusTest {
    private fun statusOf(
        profiles: List<ServerListProfile>,
        online: List<ListedPlayer> = emptyList(),
        knownAt: Map<String, String> = emptyMap(),
        rememberIp: Boolean = true,
        protocolVersion: Int = 776,
        host: String = "example.com",
        address: String? = "203.0.113.7",
    ) = ServerListStatus(
        config = { ServerListConfig(profiles, profiles.first { it.isDefault }, rememberIp) },
        placeholders = testPlaceholders(),
        slots = 20,
        versionName = "26.2",
        protocolVersion = 776,
        onlinePlayers = { online },
        configRoot = "build/test-data/config_status",
        lookupPlayerByAddress = knownAt::get,
    ).statusFor(protocolVersion, host, address)

    private fun header(
        name: String = "default",
        isDefault: Boolean = true,
        priority: Int = 0,
        conditions: List<PlaceholderCondition> = emptyList(),
    ) = ProfileHeader(name, priority, isDefault, isActive = false, conditions = conditions)

    private val plain = ServerListProfile(header(), motd = listOf("Hello"))

    @Test
    fun theCountsComeFromWhoIsActuallyConnected() {
        val status = statusOf(listOf(plain), online = listOf(ListedPlayer("Whiron", UUID)))

        assertEquals(1, status.players?.online)
        assertEquals(20, status.players?.max)
        assertEquals(
            "Whiron",
            status.players
                ?.sample
                ?.single()
                ?.name,
        )
    }

    @Test
    fun hidingThePlayerCountSendsNoCountAtAll() {
        // Not zero of twenty: the client draws ??? only when the whole object is missing.
        val status = statusOf(listOf(plain.copy(hidePlayerCount = true)))

        assertNull(status.players)
    }

    @Test
    fun placeholdersAreFilledInBeforeTheTextIsParsed() {
        // Substituted first, so `%...%` and MiniMessage's `<...>` never have to agree about syntax.
        val profile = plain.copy(motd = listOf("You speak %basalt_player_protocol% from %basalt_player_host%"))
        val status = statusOf(listOf(profile), protocolVersion = 776, host = "mc.example.com")

        assertEquals("You speak 776 from mc.example.com", PlainTextSerializer.serialize(status.description))
    }

    @Test
    fun aPlayerRecognisedByAddressIsGreetedByName() {
        val greeter =
            ServerListProfile(
                header(
                    name = "returning",
                    isDefault = false,
                    priority = 1,
                    conditions = listOf(PlaceholderCondition("%basalt_player_known%")),
                ),
                motd = listOf("Welcome back, %basalt_player_name%"),
            )
        val profiles = listOf(plain, greeter)

        val known = statusOf(profiles, knownAt = mapOf("203.0.113.7" to "Whiron"))
        assertEquals("Welcome back, Whiron", PlainTextSerializer.serialize(known.description))

        // Nobody recognised: the profile that greets by name cannot be selected at all.
        val stranger = statusOf(profiles, knownAt = emptyMap())
        assertEquals("Hello", PlainTextSerializer.serialize(stranger.description))
    }

    @Test
    fun nobodyIsLookedUpWhileTheGreetingIsOff() {
        var lookups = 0
        val status =
            ServerListStatus(
                config = { ServerListConfig(listOf(plain), plain, rememberIp = false) },
                placeholders = testPlaceholders(),
                slots = 20,
                versionName = "26.2",
                protocolVersion = 776,
                onlinePlayers = { emptyList() },
                lookupPlayerByAddress = {
                    lookups++
                    "Whiron"
                },
            ).statusFor(776, "example.com", "203.0.113.7")

        assertEquals(0, lookups)
        assertEquals("Hello", PlainTextSerializer.serialize(status.description))
    }

    @Test
    fun theTwoMotdLinesAreJoinedWithANewline() {
        val status = statusOf(listOf(plain.copy(motd = listOf("first", "second"))))

        assertEquals("first\nsecond", PlainTextSerializer.serialize(status.description))
    }

    @Test
    fun aProfileCanAdvertiseAVersionTheServerDoesNotSpeak() {
        // Cosmetic only: it changes how the list draws the row, not who may connect.
        val status = statusOf(listOf(plain.copy(versionName = "Maintenance", versionProtocol = -1)))

        assertEquals("Maintenance", status.version?.name)
        assertEquals(-1, status.version?.protocol)
    }

    @Test
    fun aPlaceholderThisPingCannotAnswerLeavesNothingRatherThanDebugText() {
        // The MOTD is read by strangers. Nobody has ever connected from this address, so there is no
        // name to greet with, and that is ordinary rather than a mistake: showing them
        // %basalt_player_name% would be showing them the inside of the config file.
        val profile = plain.copy(motd = listOf("hello %basalt_player_name%"))

        assertEquals("hello ", PlainTextSerializer.serialize(statusOf(listOf(profile)).description))
    }

    @Test
    fun aPlaceholderNobodyRegisteredStaysVisibleBecauseItIsATypo() {
        val profile = plain.copy(motd = listOf("at %basalt_nope_at_all%"))

        assertEquals("at %basalt_nope_at_all%", PlainTextSerializer.serialize(statusOf(listOf(profile)).description))
    }

    @Test
    fun theBuiltInIconIsUsedWhenAProfileNamesNoneOrNamesAMissingFile() {
        assertTrue(statusOf(listOf(plain)).favicon.contentEquals(Favicon.bytes))
        assertTrue(statusOf(listOf(plain.copy(icon = "absent.png"))).favicon.contentEquals(Favicon.bytes))
    }

    private companion object {
        const val UUID = "11111111-2222-3333-4444-555555555555"
    }
}
