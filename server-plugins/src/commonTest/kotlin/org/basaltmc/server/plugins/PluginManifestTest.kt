package org.basaltmc.server.plugins

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class PluginManifestTest {
    private val minimal =
        """
        id = "arena"
        version = "1.0.0"
        api = 1
        """.trimIndent()

    @Test
    fun `a minimal manifest is enough`() {
        val manifest = PluginManifest.parse(minimal)
        assertEquals("arena", manifest.id)
        assertEquals("arena", manifest.name, "the name falls back to the id")
        assertEquals("src/main.lua", manifest.main)
        assertEquals(PluginCapabilities.NONE, manifest.capabilities)
    }

    @Test
    fun `what is missing is named`() {
        assertTrue(
            assertFailsWith<ManifestException> {
                PluginManifest.parse("version = \"1\"\napi = 1")
            }.message!!.contains("'id'"),
        )
        assertTrue(
            assertFailsWith<ManifestException> {
                PluginManifest.parse("id = \"a\"\napi = 1")
            }.message!!.contains("'version'"),
        )
        assertTrue(
            assertFailsWith<ManifestException> {
                PluginManifest.parse("id = \"a\"\nversion = \"1\"")
            }.message!!.contains("'api'"),
        )
    }

    @Test
    fun `an id has to be usable as a namespace`() {
        for (bad in listOf("Arena", "my plugin", "2fast", "arena-pvp")) {
            assertFailsWith<ManifestException>("'$bad' should not be allowed") {
                PluginManifest.parse("id = \"$bad\"\nversion = \"1\"\napi = 1")
            }
        }
    }

    @Test
    fun `dependencies come out hard and soft`() {
        val manifest =
            PluginManifest.parse(
                """
                id = "arena"
                version = "1.0.0"
                api = 1

                [depends]
                economy = ">=1.2"

                [soft-depends]
                holograms = "*"
                """.trimIndent(),
            )
        assertEquals(2, manifest.dependencies.size)
        assertEquals(listOf("economy"), manifest.hardDependencies.map { it.id })
    }

    @Test
    fun `a requirement accepts what it says and nothing else`() {
        val atLeast = PluginDependency("x", ">=1.2", true)
        assertTrue(atLeast.accepts("1.2"))
        assertTrue(atLeast.accepts("1.2.1"))
        assertTrue(atLeast.accepts("2.0"))
        assertTrue(!atLeast.accepts("1.1.9"))
        assertTrue(PluginDependency("x", "*", true).accepts("anything"))
        assertTrue(PluginDependency("x", "1.0.0", true).accepts("1.0"))
    }

    @Test
    fun `capabilities are off unless asked for`() {
        val manifest =
            PluginManifest.parse(
                """
                id = "arena"
                version = "1.0.0"
                api = 1

                [capabilities]
                storage = true
                network = ["api.example.com"]
                """.trimIndent(),
            )
        assertTrue(manifest.capabilities.storage)
        assertTrue(!manifest.capabilities.playersData)
        assertEquals(listOf("api.example.com"), manifest.capabilities.network)
    }
}
