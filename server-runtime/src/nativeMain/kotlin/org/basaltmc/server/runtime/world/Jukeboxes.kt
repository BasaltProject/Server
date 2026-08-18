package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtString
import kotlin.concurrent.Volatile

/**
 * The block that plays a disc.
 *
 * parity: `JukeboxBlock` over `JukeboxBlockEntity`. A disc goes in, the block says so, and while it
 * plays the block gives off a full signal and a comparator reads the song's own number: eleven for
 * `11`, thirteen for `13`, and so on. Those numbers are data
 * (`data/minecraft/jukebox_song/<name>.json`), so they are read from the operator's own jar.
 */
public class Jukeboxes(
    private val serverJar: ServerJarAssets?,
) {
    @Volatile
    private var comparatorOutputs: Map<String, Int> = emptyMap()

    /** parity: `JukeboxSong.comparatorOutput`, what a comparator reads while this disc plays. */
    public fun comparatorOutputOf(song: String): Int {
        comparatorOutputs[song]?.let { return it }
        val jar = serverJar ?: return 0
        val value =
            runCatching {
                val json = jar.readText("data/minecraft/jukebox_song/$song.json")
                val at = json.indexOf("\"comparator_output\"")
                if (at < 0) return@runCatching 0
                json
                    .drop(json.indexOf(':', at) + 1)
                    .takeWhile { it.isDigit() || it == ' ' }
                    .trim()
                    .toIntOrNull() ?: 0
            }.getOrDefault(0)
        comparatorOutputs = comparatorOutputs + (song to value)
        return value
    }

    public companion object {
        public val BLOCK: Identifier = Identifier.vanilla("jukebox")

        private const val SONG_KEY = "BasaltSong"

        /** The song's own comparator number, kept beside it so a comparator needs no lookup. */
        private const val OUTPUT_KEY = "BasaltSongOutput"

        /** parity: `JukeboxBlock.HAS_RECORD` plus the disc itself, kept in the block entity. */
        public fun insert(
            world: LoadedWorld,
            x: Int,
            y: Int,
            z: Int,
            disc: Identifier,
            comparatorOutput: Int = 0,
        ): BlockChange? {
            val stateId = world.state.getBlockState(x, y, z) ?: return null
            val block = BlockStateDefinitions.blockOf(stateId) ?: return null
            if (block != BLOCK) return null
            if (BlockStateDefinitions.propertiesOf(stateId)["has_record"] == "true") return null
            val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
            val entity =
                chunk.blockEntityAt(x and 15, y, z and 15)
                    ?: org.basaltmc.server.world
                        .ChunkBlockEntity(x and 15, y, z and 15, BLOCK, NbtCompound())
                        .also { chunk.setBlockEntity(it) }
            entity.data.putString(SONG_KEY, songOf(disc))
            entity.data.putInt(OUTPUT_KEY, comparatorOutput)
            chunk.markDirty()
            val turned = BlockStateDefinitions.stateIdOf(block, mapOf("has_record" to "true"), stateId) ?: return null
            if (!world.state.setBlockState(x, y, z, turned)) return null
            return BlockChange(x, y, z, turned)
        }

        /** Takes the disc back out, which is what right-clicking a playing jukebox does. */
        public fun eject(
            world: LoadedWorld,
            x: Int,
            y: Int,
            z: Int,
        ): Pair<BlockChange, Identifier>? {
            val stateId = world.state.getBlockState(x, y, z) ?: return null
            val block = BlockStateDefinitions.blockOf(stateId) ?: return null
            if (block != BLOCK) return null
            if (BlockStateDefinitions.propertiesOf(stateId)["has_record"] != "true") return null
            val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
            val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
            val song = (entity.data[SONG_KEY] as? NbtString)?.value ?: return null
            entity.data.remove(SONG_KEY)
            entity.data.remove(OUTPUT_KEY)
            chunk.markDirty()
            val turned =
                BlockStateDefinitions.stateIdOf(block, mapOf("has_record" to "false"), stateId) ?: return null
            if (!world.state.setBlockState(x, y, z, turned)) return null
            return BlockChange(x, y, z, turned) to Identifier.vanilla("music_disc_$song")
        }

        /** What is playing here, or null when the jukebox is empty. */
        public fun songAt(
            world: LoadedWorld,
            x: Int,
            y: Int,
            z: Int,
        ): String? {
            val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
            val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
            return (entity.data[SONG_KEY] as? NbtString)?.value
        }

        /** parity: `JukeboxBlock.getAnalogOutputSignal`, the playing song's own number. */
        public fun comparatorOutputAt(data: org.basaltmc.wire.nbt.NbtCompound): Int =
            (data[OUTPUT_KEY] as? org.basaltmc.wire.nbt.NbtInt)?.value ?: 0

        /** parity: the disc item's own `jukebox_playable`, which names the song it plays. */
        public fun songOf(disc: Identifier): String = disc.path.removePrefix("music_disc_")

        /** Whether this item is a disc a jukebox takes. */
        public fun isDisc(item: Identifier): Boolean = item.path.startsWith("music_disc_")
    }
}
