package org.basaltmc.server.plugins

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A packed plugin, read the way the loader reads one.
 *
 * The zip is built here rather than committed: a binary fixture in the repository is a file nobody
 * can review, and the format is simple enough to write the handful of bytes that matter.
 */
class ArchivePackageTest {
    private val bytes =
        zipOf(
            "plugin.toml" to "id = \"packed\"\nversion = \"2.0.0\"\napi = 1\n",
            "src/main.lua" to "log.info('packed')",
            "src/lib/util.lua" to "return {}",
            "resources/assets/data/one.txt" to "first",
            "resources/assets/data/two.txt" to "second",
        )

    @Test
    fun `entries come back by name`() {
        val pkg = ArchivePackage("plugins/packed.bspkg", bytes)
        assertEquals("log.info('packed')", pkg.readText("src/main.lua"))
        assertTrue(pkg.exists("plugin.toml"))
        assertNull(pkg.read("src/missing.lua"))
    }

    @Test
    fun `a manifest read out of the archive parses`() {
        val pkg = ArchivePackage("plugins/packed.bspkg", bytes)
        val manifest = PluginManifest.parse(pkg.readText(PackageLayout.MANIFEST).orEmpty())
        assertEquals("packed", manifest.id)
        assertEquals("2.0.0", manifest.version)
    }

    @Test
    fun `listing a prefix gives what is under it in order`() {
        val pkg = ArchivePackage("plugins/packed.bspkg", bytes)
        assertEquals(
            listOf("resources/assets/data/one.txt", "resources/assets/data/two.txt"),
            pkg.list(PackageLayout.ASSETS),
        )
        assertEquals(listOf("src/lib/util.lua", "src/main.lua"), pkg.list(PackageLayout.SOURCE))
    }

    @Test
    fun `a package is not the editable form`() {
        assertTrue(!ArchivePackage("plugins/packed.bspkg", bytes).editable)
    }

    private companion object {
        fun zipOf(vararg entries: Pair<String, String>): ByteArray = TestZip.zipOf(*entries)
    }
}
