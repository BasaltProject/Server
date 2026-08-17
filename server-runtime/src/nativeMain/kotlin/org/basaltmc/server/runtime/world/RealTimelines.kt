package org.basaltmc.server.runtime.world

import kotlinx.serialization.json.Json
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.JsonToNbt
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The timelines the operator's own `server.jar` ships, as the NBT the registry packet carries.
 *
 * A timeline maps a clock's ticks to environment attributes: without it the client has a clock
 * that advances and nothing that reads it, so `/time set night` leaves the sky where it was.
 *
 * Same all-or-nothing rule as [RealDialogs] and [RealEnchantments].
 */
public class RealTimelines(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Timelines"),
) {
    public val entries: List<Pair<Identifier, NbtCompound>> = serverJar?.let(::load).orEmpty()

    public val size: Int get() = entries.size

    public val ids: List<Identifier> get() = entries.map { it.first }

    public fun definitionOf(id: Identifier): NbtCompound? = entries.firstOrNull { it.first == id }?.second

    private fun load(serverJar: ServerJarAssets): List<Pair<Identifier, NbtCompound>> {
        val loaded = mutableListOf<Pair<Identifier, NbtCompound>>()
        for (entry in serverJar.entriesUnder(TIMELINE_PREFIX).sorted()) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(TIMELINE_PREFIX).removeSuffix(".json")
            val converted =
                runCatching {
                    val root = JsonToNbt.convert(Json.parseToJsonElement(serverJar.readText(entry))) as NbtCompound
                    syncable(root)
                }.getOrElse { error ->
                    logger.warn {
                        "Timeline '$path' could not be converted (${error.message}); sending no timeline " +
                            "registry, because a partial one would disconnect clients at login."
                    }
                    return emptyList()
                }
            loaded += Identifier.vanilla(path) to converted
        }
        if (loaded.isNotEmpty()) logger.info { "Loaded ${loaded.size} timeline(s)." }
        return loaded
    }

    /** parity: `Timeline.filterSyncableTracks`, which is the only thing `NETWORK_CODEC` changes. */
    private fun syncable(root: NbtCompound): NbtCompound {
        val tracks = root["tracks"] as? NbtCompound ?: return root
        val kept = NbtCompound()
        for (key in tracks.keys) {
            val value = tracks[key] ?: continue
            if (key.removePrefix("minecraft:") in SYNCABLE_ATTRIBUTES) kept[key] = value
        }
        val result = NbtCompound()
        for (key in root.keys) root[key]?.let { result[key] = it }
        if (kept.keys.isEmpty()) result.remove("tracks") else result["tracks"] = kept
        return result
    }

    private companion object {
        const val TIMELINE_PREFIX = "data/minecraft/timeline/"

        /** parity: the 33 of `EnvironmentAttributes` built with `syncable()`. */
        val SYNCABLE_ATTRIBUTES =
            setOf(
                "audio/ambient_sounds",
                "audio/background_music",
                "audio/firefly_bush_sounds",
                "audio/music_volume",
                "gameplay/can_start_raid",
                "gameplay/creaking_active",
                "gameplay/fast_lava",
                "gameplay/piglins_zombify",
                "gameplay/sky_light_level",
                "visual/ambient_light_color",
                "visual/ambient_particles",
                "visual/block_light_tint",
                "visual/cloud_color",
                "visual/cloud_fog_end_distance",
                "visual/cloud_height",
                "visual/default_dripstone_particle",
                "visual/fog_color",
                "visual/fog_end_distance",
                "visual/fog_start_distance",
                "visual/moon_angle",
                "visual/moon_phase",
                "visual/night_vision_color",
                "visual/sky_color",
                "visual/sky_fog_end_distance",
                "visual/sky_light_color",
                "visual/sky_light_factor",
                "visual/star_angle",
                "visual/star_brightness",
                "visual/sun_angle",
                "visual/sunrise_sunset_color",
                "visual/water_fog_color",
                "visual/water_fog_end_distance",
                "visual/water_fog_start_distance",
            )
    }
}
