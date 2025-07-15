package org.basaltmc.server.plugins

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PluginOrderTest {
    private fun manifest(
        id: String,
        depends: List<String> = emptyList(),
        soft: List<String> = emptyList(),
        api: Int = PluginManifest.API,
        version: String = "1.0.0",
    ) = PluginManifest(
        id = id,
        name = id,
        version = version,
        authors = emptyList(),
        api = api,
        main = "src/main.lua",
        dependencies =
            depends.map { PluginDependency(it, "*", true) } +
                soft.map { PluginDependency(it, "*", false) },
        capabilities = PluginCapabilities.NONE,
        privateResources = emptyList(),
    )

    private fun loadedIds(decisions: List<PluginOrdering>) =
        decisions.filterIsInstance<PluginOrdering.Load>().map { it.manifest.id }

    private fun refusals(decisions: List<PluginOrdering>) =
        decisions.filterIsInstance<PluginOrdering.Refused>().associate { it.manifest.id to it.reason }

    @Test
    fun `what depends on something loads after it`() {
        val decisions = PluginOrder.resolve(listOf(manifest("shop", depends = listOf("economy")), manifest("economy")))
        assertEquals(listOf("economy", "shop"), loadedIds(decisions))
    }

    @Test
    fun `soft dependencies only decide the order`() {
        val decisions = PluginOrder.resolve(listOf(manifest("shop", soft = listOf("holograms")), manifest("holograms")))
        assertEquals(listOf("holograms", "shop"), loadedIds(decisions))
        val alone = PluginOrder.resolve(listOf(manifest("shop", soft = listOf("holograms"))))
        assertEquals(listOf("shop"), loadedIds(alone), "a missing soft dependency is not a refusal")
    }

    @Test
    fun `independent plugins load in a fixed order`() {
        val decisions = PluginOrder.resolve(listOf(manifest("zebra"), manifest("alpha"), manifest("mid")))
        assertEquals(listOf("alpha", "mid", "zebra"), loadedIds(decisions))
    }

    @Test
    fun `a missing dependency is refused with its name`() {
        val decisions = PluginOrder.resolve(listOf(manifest("shop", depends = listOf("economy"))))
        assertTrue(refusals(decisions)["shop"]!!.contains("economy"))
    }

    @Test
    fun `a version that does not match is refused with both versions`() {
        val decisions =
            PluginOrder.resolve(
                listOf(
                    manifest("shop").copy(dependencies = listOf(PluginDependency("economy", ">=2.0", true))),
                    manifest("economy", version = "1.4.0"),
                ),
            )
        val reason = refusals(decisions)["shop"]!!
        assertTrue(reason.contains(">=2.0") && reason.contains("1.4.0"), reason)
    }

    @Test
    fun `what depended on a refused plugin is refused too`() {
        val decisions =
            PluginOrder.resolve(
                listOf(
                    manifest("front", depends = listOf("shop")),
                    manifest("shop", depends = listOf("economy")),
                ),
            )
        assertEquals(emptyList(), loadedIds(decisions))
        assertTrue(refusals(decisions)["front"]!!.contains("shop"))
    }

    @Test
    fun `a cycle is refused as a group and the group is named`() {
        val decisions =
            PluginOrder.resolve(
                listOf(manifest("a", depends = listOf("b")), manifest("b", depends = listOf("a"))),
            )
        assertEquals(emptyList(), loadedIds(decisions))
        assertTrue(refusals(decisions)["a"]!!.contains("a, b"), refusals(decisions).toString())
    }

    @Test
    fun `a plugin for another api major does not load`() {
        val decisions = PluginOrder.resolve(listOf(manifest("old", api = PluginManifest.API + 1)))
        assertTrue(refusals(decisions)["old"]!!.contains("API"))
    }

    @Test
    fun `unloading names what would break furthest first`() {
        val loaded =
            listOf(
                manifest("economy"),
                manifest("shop", depends = listOf("economy")),
                manifest("front", depends = listOf("shop")),
            )
        assertEquals(listOf("front", "shop"), PluginOrder.dependents("economy", loaded))
        assertEquals(emptyList(), PluginOrder.dependents("front", loaded))
    }
}
