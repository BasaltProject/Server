package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.world.WorldHeight

/**
 * Hand-authored NBT for the two registries the vertical slice's flat Overworld actually needs
 * during Configuration sync (`minecraft:dimension_type`, `minecraft:worldgen/biome`). Values
 * match the well-known modern Overworld dimension type and a plain plains biome; not yet
 * cross-checked against the real 26.2 jar (same provisional status as `Blocks`/`Biomes` in
 * `basalt-registry`). The full registry set (damage_type, chat_type, and the rest a real vanilla
 * client also expects) is a documented gap: `basalt-datagen` generates it properly later.
 */
public object VanillaRegistryData {
    public fun overworldDimensionType(): NbtCompound =
        NbtCompound().apply {
            putBoolean("piglin_safe", false)
            putBoolean("natural", true)
            putFloat("ambient_light", 0.0f)
            putInt("monster_spawn_block_light_limit", 0)
            putInt("monster_spawn_light_level", 0)
            putString("infiniburn", "#minecraft:infiniburn_overworld")
            putBoolean("respawn_anchor_works", false)
            putBoolean("has_skylight", true)
            putBoolean("bed_works", true)
            putString("effects", "minecraft:overworld")
            putBoolean("has_raids", true)
            putInt("logical_height", WorldHeight.HEIGHT)
            putInt("min_y", WorldHeight.MIN_Y)
            putInt("height", WorldHeight.HEIGHT)
            putDouble("coordinate_scale", 1.0)
            putBoolean("ultrawarm", false)
            putBoolean("has_ceiling", false)
        }

    public fun plainsBiome(): NbtCompound =
        NbtCompound().apply {
            putBoolean("has_precipitation", true)
            putFloat("temperature", 0.8f)
            putFloat("downfall", 0.4f)
            val effects =
                NbtCompound().apply {
                    putInt("fog_color", 12638463)
                    putInt("water_color", 4159204)
                    putInt("water_fog_color", 329011)
                    putInt("sky_color", 7907327)
                }
            this["effects"] = effects
        }
}
