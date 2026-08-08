package org.basaltmc.server.registry

import org.basaltmc.registry.Biomes
import kotlin.test.Test
import kotlin.test.assertEquals
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
}
