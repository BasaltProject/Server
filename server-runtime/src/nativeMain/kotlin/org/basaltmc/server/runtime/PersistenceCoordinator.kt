package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.wire.common.logging.Logger

/**
 * Everything that writes the server's state to disk on a schedule: players, worlds, and the
 * chunks nobody can reach any more. Split out of [BasaltServer]; [TickLoop] and [SaveControl]
 * still decide *when* this runs, this only decides what running means.
 *
 * [worlds] and [pluginEvents] are read live rather than captured once: a plugin can add a world
 * or a listener after this is built, and both must see it on the next save or sweep.
 */
public class PersistenceCoordinator(
    private val worlds: () -> Map<String, LoadedWorld>,
    private val playerRegistry: PlayerRegistry,
    private val playerStore: PlayerStore,
    private val commandStorage: CommandStorage,
    private val pluginEvents: () -> PluginEventBus,
    private val metrics: ServerMetrics,
    private val logger: Logger,
    private val spawnRadiusChunks: Int,
    private val retainMarginChunks: Int,
) {
    /** Writes every online player as one transaction, on the same interval the worlds save on. */
    internal fun persistPlayers() {
        val snapshots = playerRegistry.snapshot().mapNotNull { it.connection.storedSnapshot() }
        if (snapshots.isEmpty()) return
        runCatching { playerStore.players.saveAll(snapshots) }
            .onFailure { logger.error(it) { "Failed to save ${snapshots.size} player(s)" } }
    }

    /** parity: `MinecraftServer.saveEverything`, the whole autosave: every world, then the players. */
    internal fun persistEverything() {
        // Emitted before the work, not after: if this method ever stops completing, the last thing
        // in the log is where it got to, instead of nothing at all.
        logger.debug { "Save starting across ${worlds().size} world(s)." }
        for (world in worlds().values) {
            world.saveDirty()
            // Clock, weather and game rules, which no chunk carries: cheap enough to write on every
            // save, and losing an hour of world time to a crash is worse than one small file write.
            runCatching { commandStorage.save() }
                .onFailure { logger.error(it) { "Could not save command storage" } }
            runCatching { world.saveMetadata() }
                .onFailure { logger.error(it) { "Could not save level.nbt for world '${world.name}'" } }
        }
        persistPlayers()
    }

    /** Chunks nobody can reach go, whatever changed in them written on the way out. */
    internal fun unloadUnreachableChunks() {
        var loaded = 0
        var unloaded = 0
        for (world in worlds().values) {
            // Only the players actually in this world keep its chunks resident.
            val positions =
                playerRegistry
                    .snapshot()
                    .filter { it.worldName == world.name }
                    .map { (it.x.toInt() shr 4) to (it.z.toInt() shr 4) }
            val before =
                if (pluginEvents().listening(PluginEventTypes.WORLD_CHUNK_UNLOAD)) {
                    world.state.loadedChunkKeys()
                } else {
                    null
                }
            unloaded += world.unloadUnreachable(positions, spawnRadiusChunks, retainMarginChunks)
            if (before != null) {
                val after = world.state.loadedChunkKeys()
                for (key in before) {
                    if (key in after) continue
                    pluginEvents().emit(PluginEventTypes.WORLD_CHUNK_UNLOAD) { event ->
                        event.set(0, world.name)
                        event.set(1, (key shr 32).toInt())
                        event.set(2, key.toInt())
                    }
                }
            }
            loaded += world.state.loadedChunkCount
        }

        // Reported every sweep, not only when something happens: when this server misbehaves it is
        // because one of these numbers climbs without settling, and knowing which one is the whole
        // diagnosis. At debug level because it is a diagnostic, not operator news.
        logger.debug {
            "Chunks: $loaded loaded across ${worlds().size} world(s), $unloaded unloaded. " + metrics.heapReport()
        }
    }

    /**
     * The UUID Mojang has for a name, or null when there is none or the lookup failed.
     *
     * Online mode is not required for this: `/fetchprofile` is how an operator finds out who a name
     * belongs to before letting them in, which matters most on a server that is not checking.
     */
    internal fun fetchMojangUuid(name: String): String? =
        runCatching { MojangProfileLookup.uuidOf(name) }
            .onFailure { logger.debug { "Profile lookup for '$name' failed: ${it.message}" } }
            .getOrNull()
}
