package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.registry.Biomes
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class VanillaBiomesTest {
    @Test
    fun allEntriesMatchTheBiomeRegistryOrder() {
        assertEquals(Biomes.REGISTRY.size, VanillaBiomes.ALL.size)
        VanillaBiomes.ALL.forEachIndexed { id, entry ->
            assertEquals("minecraft:${entry.path}", Biomes.REGISTRY.identifierOf(id)?.toString())
        }
    }

    @Test
    fun everyEntrySerializesItsClimateAndEffects() {
        for (entry in VanillaBiomes.ALL) {
            val nbt = with(VanillaBiomes) { entry.toNbt() }
            assertTrue(nbt.contains("has_precipitation"), "${entry.path} misses has_precipitation")
            assertTrue(nbt.contains("temperature"), "${entry.path} misses temperature")
            assertTrue(nbt.contains("downfall"), "${entry.path} misses downfall")
            assertTrue(nbt.contains("effects"), "${entry.path} misses effects")
            val effects = nbt.getCompound("effects")
            assertTrue(effects.contains("water_color"), "${entry.path} misses effects.water_color")
        }
    }

    @Test
    fun frozenOceansCarryTheFrozenTemperatureModifier() {
        val frozen = VanillaBiomes.ALL.filter { it.temperatureModifier == "frozen" }.map { it.path }
        assertEquals(listOf("frozen_ocean", "deep_frozen_ocean"), frozen)
    }

    @Test
    fun theVoidNetherAndEndDefinitionsMatchTheOfficialData() {
        val byPath = VanillaBiomes.ALL.associateBy { it.path }
        assertEquals(66, byPath.size)

        val theVoid = with(VanillaBiomes) { byPath.getValue("the_void").toNbt() }
        assertFalse(theVoid.getBoolean("has_precipitation"))
        assertEquals("#7ba4ff", theVoid.getCompound("attributes").getString("minecraft:visual/sky_color"))

        val warped = with(VanillaBiomes) { byPath.getValue("warped_forest").toNbt() }
        val attributes = warped.getCompound("attributes")
        assertEquals("#1a051a", attributes.getString("minecraft:visual/fog_color"))
        assertEquals(
            "minecraft:music.nether.warped_forest",
            attributes
                .getCompound("minecraft:audio/background_music")
                .getCompound("default")
                .getString("sound"),
        )
        val sounds = attributes.getCompound("minecraft:audio/ambient_sounds")
        assertEquals("minecraft:ambient.warped_forest.loop", sounds.getString("loop"))
        assertEquals("minecraft:ambient.warped_forest.mood", sounds.getCompound("mood").getString("sound"))
        assertEquals(
            "minecraft:ambient.warped_forest.additions",
            sounds.getCompound("additions").getString("sound"),
        )
        val particles = attributes.getList("minecraft:visual/ambient_particles")
        assertEquals(1, particles?.size)
        val particle = particles?.get(0) as NbtCompound
        assertEquals("minecraft:warped_spore", particle.getCompound("particle").getString("type"))
        assertEquals(0.01428f, particle.getFloat("probability"))

        for (path in listOf("the_end", "end_highlands", "end_midlands", "small_end_islands", "end_barrens")) {
            val end = with(VanillaBiomes) { byPath.getValue(path).toNbt() }
            assertFalse(end.getBoolean("has_precipitation"), path)
            assertEquals(0.5f, end.getFloat("temperature"), path)
            assertEquals("#3f76e4", end.getCompound("effects").getString("water_color"), path)
        }
    }
}
