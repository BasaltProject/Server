package org.basaltmc.server.runtime.config

import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class LiveConfigsTest {
    /**
     * A directory with nothing left in it from a previous run.
     *
     * These tests write deliberately broken files, and build/test-data survives between runs: one
     * of them left a broken scoreboard.toml behind and the next run failed before it had started.
     */
    private fun directoryFor(name: String): ConfigDirectory {
        val directory = ConfigDirectory("build/test-data/config_live_$name", Logger("Test"))
        FileIO.createDirectories(directory.root)
        for (file in LiveConfigs.RELOADABLE_FILES) FileIO.delete(directory.pathOf(file))
        return directory
    }

    private fun write(
        directory: ConfigDirectory,
        file: String,
        contents: String,
    ) {
        FileIO.writeText(directory.pathOf(file), contents)
    }

    @Test
    fun readingGeneratesEveryReloadableFileAndNothingElse() {
        val directory = directoryFor("generate")

        LiveConfigs.read(directory, Logger("Test"))

        for (file in LiveConfigs.RELOADABLE_FILES) {
            assertTrue(FileIO.exists(directory.pathOf(file)), "$file should have been generated")
        }
        // The two that are read once at startup are not this class's business, and generating them
        // here would mean two places create them.
        assertFalse(FileIO.exists(directory.pathOf(BasaltConfig.FILE_NAME)))
        assertFalse(FileIO.exists(directory.pathOf(WorldsConfig.FILE_NAME)))
    }

    @Test
    fun everythingIsSwappedTogetherOrNotAtAll() {
        val directory = directoryFor("swap")
        val first = LiveConfigs.read(directory, Logger("Test"))
        val live = LiveConfigs(first)

        write(directory, GroupsConfig.FILE_NAME, GROUPS_DEFAULT_TOML.replace("basalt.command.help", "basalt.new"))
        write(directory, TabListConfig.FILE_NAME, TAB_LIST_DEFAULT_TOML.replace("enabled = true", "enabled = false"))
        live.replace(LiveConfigs.read(directory, Logger("Test")))

        // One atomic swap: a reader never sees the new groups with the old tab list.
        assertTrue(live.current.groups.resolve("default", "basalt.new"))
        assertFalse(live.current.ui.tabList.enabled)
    }

    @Test
    fun aFileThatDoesNotParseLeavesTheRunningConfigurationAlone() {
        val directory = directoryFor("broken")
        val live = LiveConfigs(LiveConfigs.read(directory, Logger("Test")))
        val before = live.current

        write(directory, ScoreboardConfig.FILE_NAME, "this is not = = toml")

        // Read before applied: three good files and one broken one must not leave the server in a
        // state no file on disk describes.
        assertFailsWith<TomlFormatException> { LiveConfigs.read(directory, Logger("Test")) }
        assertEquals(before, live.current)
    }

    @Test
    fun theSnapshotIsWhatEveryReaderSees() {
        val directory = directoryFor("readers")
        val live = LiveConfigs(LiveConfigs.read(directory, Logger("Test")))

        val seenBefore = live.current.ui
        live.replace(live.current.copy(ui = UiConfigs()))

        // Readers hold the holder, not the snapshot, so they follow the swap.
        assertTrue(seenBefore !== live.current.ui)
    }
}
