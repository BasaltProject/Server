package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.io.FileIO
import org.basaltmc.protocol.play.BossBarColor
import org.basaltmc.protocol.play.BossBarOverlay
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

class UiConfigTest {
    private val registry =
        PlaceholderRegistry().apply {
            register("basalt_player") { params, _ ->
                when (params) {
                    "dimension" -> PlaceholderValue.of("the_nether")
                    "is_op" -> PlaceholderValue.of(true)
                    else -> PlaceholderValue.Unknown
                }
            }
        }

    private fun directoryFor(name: String) = ConfigDirectory("build/test-data/config_ui_$name", Logger("Test"))

    private fun write(
        name: String,
        file: String,
        contents: String,
    ): ConfigDirectory {
        val directory = directoryFor(name)
        FileIO.createDirectories(directory.root)
        FileIO.writeText(directory.pathOf(file), contents)
        return directory
    }

    @Test
    fun theTabListIsTheOnlyOneOnByDefault() {
        val tabList = TabListConfig.load(write("defaults_tab", TabListConfig.FILE_NAME, TAB_LIST_DEFAULT_TOML))
        val scoreboard =
            ScoreboardConfig.load(write("defaults_sb", ScoreboardConfig.FILE_NAME, SCOREBOARD_DEFAULT_TOML))
        val bossBar = BossBarConfig.load(write("defaults_bb", BossBarConfig.FILE_NAME, BOSS_BAR_DEFAULT_TOML))

        // Decoration that cannot cover the screen is on; the two that take up the view are not.
        assertTrue(tabList.enabled)
        assertFalse(scoreboard.enabled)
        assertFalse(bossBar.enabled)
    }

    @Test
    fun theShippedTabListHasAnEmptyFirstLineAndTheServerName() {
        val config = TabListConfig.load(write("tab_shape", TabListConfig.FILE_NAME, TAB_LIST_DEFAULT_TOML))
        val header = assertNotNull(config.defaultProfile.headerLines)

        assertEquals(" ", header.first())
        assertTrue(header.any { it.contains("BASALT SERVER") && it.contains("<gradient:") })
    }

    @Test
    fun theShippedFooterReportsTheServerItself() {
        val config = TabListConfig.load(write("tab_footer", TabListConfig.FILE_NAME, TAB_LIST_DEFAULT_TOML))
        val footer = assertNotNull(config.defaultProfile.footerLines).joinToString("\n")

        for (name in listOf("%basalt_server_tps%", "%basalt_server_ram_used%", "%basalt_server_cpu%")) {
            assertTrue(name in footer, "the footer should report $name")
        }
    }

    @Test
    fun theScoreboardShipsAProfilePerDimension() {
        val directory = write("sb_dims", ScoreboardConfig.FILE_NAME, SCOREBOARD_DEFAULT_TOML)
        val config = ScoreboardConfig.load(directory)

        val nether =
            config.profiles.selectProfile(
                config.defaultProfile,
                registry,
                PlaceholderContext.EMPTY,
                Logger("Test"),
                OnceReporter(),
                "scoreboard",
            )
        // The registry above says the player is in the Nether, so that profile wins over the default.
        assertEquals("nether", nether.name)
        assertTrue(config.profiles.any { it.name == "end" })
    }

    @Test
    fun aProfileTakesWhatItDoesNotDeclareFromTheDefaultOne() {
        val directory =
            write(
                "sb_inherit",
                ScoreboardConfig.FILE_NAME,
                """
                enabled = true
                [profiles.default]
                default = true
                title = "base"
                lines = ["one", "two"]

                [profiles.other]
                active = true
                title = "other"
                """.trimIndent(),
            )
        val config = ScoreboardConfig.load(directory)
        val selected =
            config.profiles
                .selectProfile(
                    config.defaultProfile,
                    registry,
                    PlaceholderContext.EMPTY,
                    Logger("Test"),
                    OnceReporter(),
                    "scoreboard",
                ).inheriting(config.defaultProfile)

        assertEquals("other", selected.title)
        assertEquals(listOf("one", "two"), selected.lines)
    }

    @Test
    fun theBossBarReadsItsColourAndOverlayByName() {
        val directory =
            write(
                "bb_style",
                BossBarConfig.FILE_NAME,
                """
                enabled = true
                [profiles.default]
                default = true
                text = "hello"
                color = "PURPLE"
                overlay = "notched_10"
                progress = "0.5"
                """.trimIndent(),
            )
        val config = BossBarConfig.load(directory)

        assertEquals(BossBarColor.PURPLE, config.defaultProfile.color)
        assertEquals(BossBarOverlay.NOTCHED_10, config.defaultProfile.overlay)
        assertEquals("0.5", config.defaultProfile.progress)
    }

    @Test
    fun anUnknownColourFallsBackInsteadOfStoppingTheServer() {
        val directory =
            write(
                "bb_bad",
                BossBarConfig.FILE_NAME,
                "[profiles.default]\ndefault = true\ntext = \"x\"\ncolor = \"turquoise\"\n",
            )

        // One wrong word in decoration is not a file that cannot be understood.
        assertEquals(BossBarColor.WHITE, BossBarConfig.load(directory).defaultProfile.color)
    }

    @Test
    fun aUiFileWithoutExactlyOneDefaultProfileStopsTheServer() {
        val directory = write("no_default", TabListConfig.FILE_NAME, "[profiles.a]\nheader = [\"x\"]\n")

        kotlin.test.assertFailsWith<TomlFormatException> { TabListConfig.load(directory) }
    }
}
