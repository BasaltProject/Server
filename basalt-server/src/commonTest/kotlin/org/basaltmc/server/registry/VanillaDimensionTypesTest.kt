package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtString
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class VanillaDimensionTypesTest {
    @Test
    fun bothReferencedWorldClocksAreSynced() {
        assertEquals(listOf("overworld", "the_end"), VanillaRegistryData.worldClocks.map { it.first })
        assertTrue(VanillaRegistryData.worldClocks.all { it.second.size == 0 })
    }

    @Test
    fun allFourBuiltinTypesUseBootstrapOrderAndBounds() {
        assertEquals(
            listOf("overworld", "the_nether", "the_end", "overworld_caves"),
            VanillaRegistryData.dimensionTypes.map { it.first },
        )
        val types = VanillaRegistryData.dimensionTypes.associate { it.first to it.second }

        val overworld = types.getValue("overworld")
        assertEquals(-64, overworld.getInt("min_y"))
        assertEquals(384, overworld.getInt("height"))
        assertFalse(overworld.getBoolean("has_ceiling"))
        assertEquals("minecraft:uniform", overworld.getCompound("monster_spawn_light_level").getString("type"))

        val caves = types.getValue("overworld_caves")
        assertTrue(caves.getBoolean("has_ceiling"))
        assertEquals(overworld.getCompound("attributes"), caves.getCompound("attributes"))
    }

    @Test
    fun netherAndEndCarryTheirExactDimensionRules() {
        val types = VanillaRegistryData.dimensionTypes.associate { it.first to it.second }
        val nether = types.getValue("the_nether")
        assertTrue(nether.getBoolean("has_fixed_time"))
        assertFalse(nether.getBoolean("has_skylight"))
        assertEquals(8.0, nether.getDouble("coordinate_scale"))
        assertEquals(128, nether.getInt("logical_height"))
        assertEquals(NbtString("#minecraft:infiniburn_nether"), nether["infiniburn"])
        assertTrue(nether.getCompound("attributes").getBoolean("minecraft:gameplay/fast_lava"))

        val end = types.getValue("the_end")
        assertTrue(end.getBoolean("has_ender_dragon_fight"))
        assertEquals("end", end.getString("skybox"))
        assertEquals(
            "minecraft:music.end",
            end
                .getCompound("attributes")
                .getCompound("minecraft:audio/background_music")
                .getCompound("default")
                .getString("sound"),
        )
    }
}
