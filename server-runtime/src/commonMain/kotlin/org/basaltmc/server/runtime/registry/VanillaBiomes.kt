package org.basaltmc.server.runtime.registry

import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType

/**
 * Every biome as a 26.2 server syncs it during Configuration: only what `Biome.NETWORK_CODEC`
 * carries, since spawners, features and carvers stay server-side. Entry order matches
 * [org.basaltmc.server.registry.Biomes], because each entry's index is the id a chunk's biome palette
 * references.
 *
 * Colors serialize as `#rrggbb`, which is what `ExtraCodecs.STRING_RGB_COLOR` writes; it also
 * decodes plain ints, which is why the earlier int-encoded form worked anyway.
 */
public object VanillaBiomes {
    /** One environment attribute value, in the shapes the real biome data actually uses. */
    public sealed interface BiomeAttribute {
        public data class Color(
            val hex: String,
        ) : BiomeAttribute

        public data class Flag(
            val value: Boolean,
        ) : BiomeAttribute

        public data class Number(
            val value: Float,
        ) : BiomeAttribute

        /** A real attribute modifier object (`{"argument": ..., "modifier": "multiply"}`), e.g. `visual/water_fog_end_distance`. */
        public data class Modifier(
            val argument: Float,
            val modifier: String,
        ) : BiomeAttribute

        /** `audio/background_music`: per-slot tracks (`default`/`creative`/`underwater`). */
        public data class Music(
            val tracks: Map<String, MusicTrack>,
        ) : BiomeAttribute

        public data class Particles(
            val particles: List<AmbientParticle>,
        ) : BiomeAttribute

        public data class Sounds(
            val loop: String,
            val mood: AmbientMood,
            val addition: AmbientAddition,
        ) : BiomeAttribute
    }

    public data class MusicTrack(
        val sound: String,
        val minDelay: Int,
        val maxDelay: Int,
    )

    public data class AmbientParticle(
        val type: String,
        val probability: Float,
    )

    public data class AmbientMood(
        val sound: String,
        val tickDelay: Int = 6000,
        val blockSearchExtent: Int = 8,
        val offset: Double = 2.0,
    )

    public data class AmbientAddition(
        val sound: String,
        val tickChance: Double = 0.0111,
    )

    public data class Entry(
        val path: String,
        val temperature: Float,
        val downfall: Float,
        val hasPrecipitation: Boolean,
        val waterColor: String,
        val temperatureModifier: String? = null,
        val grassColor: String? = null,
        val foliageColor: String? = null,
        val dryFoliageColor: String? = null,
        val grassColorModifier: String? = null,
        val attributes: Map<String, BiomeAttribute> = emptyMap(),
    )

    /**
     * Every registered biome, in [org.basaltmc.server.registry.Biomes] registration order.
     * Split across [VanillaOverworldBiomesA], [VanillaOverworldBiomesB] and
     * [VanillaNetherEndBiomes] for file size; the concatenation order below must match the
     * original single-list order exactly, since it is the network id a chunk's biome palette
     * references.
     */
    public val ALL: List<Entry> =
        VanillaOverworldBiomesA.ENTRIES + VanillaOverworldBiomesB.ENTRIES +
            VanillaNetherEndBiomes.NETHER_ENTRIES + VanillaNetherEndBiomes.END_ENTRIES

    /** Serializes one entry to the exact NBT a vanilla server sends for it in `RegistryDataPacket`. */
    public fun Entry.toNbt(): NbtCompound =
        NbtCompound().apply {
            putBoolean("has_precipitation", hasPrecipitation)
            putFloat("temperature", temperature)
            temperatureModifier?.let { putString("temperature_modifier", it) }
            putFloat("downfall", downfall)
            if (attributes.isNotEmpty()) {
                this["attributes"] =
                    NbtCompound().apply {
                        for ((key, value) in attributes) {
                            when (value) {
                                is BiomeAttribute.Color -> putString(key, value.hex)
                                is BiomeAttribute.Flag -> putBoolean(key, value.value)
                                is BiomeAttribute.Number -> putFloat(key, value.value)
                                is BiomeAttribute.Modifier ->
                                    this[key] =
                                        NbtCompound().apply {
                                            putFloat("argument", value.argument)
                                            putString("modifier", value.modifier)
                                        }
                                is BiomeAttribute.Music ->
                                    this[key] =
                                        NbtCompound().apply {
                                            for ((slot, track) in value.tracks) {
                                                this[slot] =
                                                    NbtCompound().apply {
                                                        putString("sound", track.sound)
                                                        putInt("min_delay", track.minDelay)
                                                        putInt("max_delay", track.maxDelay)
                                                    }
                                            }
                                        }
                                is BiomeAttribute.Particles ->
                                    this[key] =
                                        NbtList(
                                            NbtType.COMPOUND,
                                            value.particles.map { particle ->
                                                NbtCompound().apply {
                                                    this["particle"] =
                                                        NbtCompound().apply {
                                                            putString("type", particle.type)
                                                        }
                                                    putFloat("probability", particle.probability)
                                                }
                                            },
                                        )
                                is BiomeAttribute.Sounds ->
                                    this[key] =
                                        NbtCompound().apply {
                                            putString("loop", value.loop)
                                            this["mood"] =
                                                NbtCompound().apply {
                                                    putString("sound", value.mood.sound)
                                                    putInt("tick_delay", value.mood.tickDelay)
                                                    putInt("block_search_extent", value.mood.blockSearchExtent)
                                                    putDouble("offset", value.mood.offset)
                                                }
                                            this["additions"] =
                                                NbtCompound().apply {
                                                    putString("sound", value.addition.sound)
                                                    putDouble("tick_chance", value.addition.tickChance)
                                                }
                                        }
                            }
                        }
                    }
            }
            this["effects"] =
                NbtCompound().apply {
                    putString("water_color", waterColor)
                    foliageColor?.let { putString("foliage_color", it) }
                    dryFoliageColor?.let { putString("dry_foliage_color", it) }
                    grassColor?.let { putString("grass_color", it) }
                    grassColorModifier?.let { putString("grass_color_modifier", it) }
                }
        }
}
