package org.basaltmc.server.runtime.registry

import kotlin.test.Test
import kotlin.test.assertEquals

class VanillaEntityVariantsTest {
    @Test
    fun registryOrdersMatchVanillaBootstrapIds() {
        assertEquals(
            listOf("pale", "spotted", "snowy", "black", "ashen", "rusty", "woods", "chestnut", "striped"),
            VanillaRegistryData.wolfVariants,
        )
        assertEquals(
            listOf("classic", "puglin", "sad", "angry", "grumpy", "big", "cute"),
            VanillaRegistryData.wolfSoundVariants,
        )
        assertEquals(listOf("temperate", "warm", "cold"), VanillaRegistryData.cowVariants)
        assertEquals(listOf("temperate", "warm", "cold"), VanillaRegistryData.pigVariants)
        assertEquals(listOf("temperate", "warm", "cold"), VanillaRegistryData.chickenVariants)
        assertEquals(listOf("temperate", "warm", "cold"), VanillaRegistryData.frogVariants)
        assertEquals(listOf("classic", "big", "mini"), VanillaRegistryData.pigSoundVariants)
    }

    @Test
    fun networkEntriesRetainExactModelsTexturesAndFixedSteps() {
        assertEquals("cold", VanillaRegistryData.chickenVariant("cold").getString("model"))
        assertEquals("warm", VanillaRegistryData.cowVariant("warm").getString("model"))
        assertEquals("cold", VanillaRegistryData.pigVariant("cold").getString("model"))
        assertEquals(
            "minecraft:entity/wolf/wolf",
            VanillaRegistryData.wolfVariant("pale").getCompound("assets").getString("wild"),
        )
        assertEquals(
            "minecraft:entity.wolf.step",
            VanillaRegistryData.wolfSoundVariant("puglin").getCompound("adult_sounds").getString("step_sound"),
        )
        assertEquals(
            "minecraft:entity.chicken.step",
            VanillaRegistryData.chickenSoundVariant("picky").getCompound("adult_sounds").getString("step_sound"),
        )
        assertEquals(
            "minecraft:entity.pig.step",
            VanillaRegistryData.pigSoundVariant("big").getCompound("adult_sounds").getString("step_sound"),
        )
    }
}
