package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.NbtType
import org.basaltmc.world.WorldHeight

/**
 * Hand-authored NBT for the two registries the vertical slice's flat Overworld actually needs
 * during Configuration sync (`minecraft:dimension_type`, `minecraft:worldgen/biome`). Field
 * shapes come from decompiling `DimensionType`/`Biome` out of the real 26.2 server jar (their
 * schemas changed substantially from older, widely-known Minecraft versions: no more
 * `piglin_safe`/`natural`/`ultrawarm`/`bed_works`/`has_raids`/`respawn_anchor_works`, and biome
 * `effects` dropped `fog_color`/`sky_color`/`water_fog_color`), not guessed from memory. The full
 * registry set (`damage_type`, `chat_type`, and the rest a real vanilla client also expects) is a
 * documented gap: `basalt-datagen` generates it properly later.
 */
public object VanillaRegistryData {
    public fun overworldDimensionType(): NbtCompound =
        NbtCompound().apply {
            putBoolean("has_skylight", true)
            putBoolean("has_ceiling", false)
            putBoolean("has_ender_dragon_fight", false)
            putDouble("coordinate_scale", 1.0)
            putInt("min_y", WorldHeight.MIN_Y)
            putInt("height", WorldHeight.HEIGHT)
            putInt("logical_height", WorldHeight.HEIGHT)
            // A literal block id, not a "#tag" reference: tags need a `tags` packet (not sent yet) to
            // resolve against, but static registries like Block are never network-synced, so a plain
            // id resolves against the client's own built-in registry regardless of known-packs state.
            this["infiniburn"] = NbtList(NbtType.STRING, listOf(NbtString("minecraft:magma_block")))
            putFloat("ambient_light", 0.0f)
            // IntProviders.codec's Either<Int, IntProvider>: a bare int is shorthand for a constant provider.
            putInt("monster_spawn_light_level", 0)
            putInt("monster_spawn_block_light_limit", 0)
        }

    public fun plainsBiome(): NbtCompound =
        NbtCompound().apply {
            putBoolean("has_precipitation", true)
            putFloat("temperature", 0.8f)
            putFloat("downfall", 0.4f)
            val effects = NbtCompound().apply { putInt("water_color", 4159204) }
            this["effects"] = effects
        }
}
