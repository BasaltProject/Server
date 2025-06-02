package org.basaltmc.server.world

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import kotlin.random.Random

/**
 * What a world remembers about itself between restarts: `worlds/<name>/level.nbt`. The seed a world
 * was generated with is state, not configuration: left in a config file, editing it regenerates
 * different terrain next to the terrain already on disk.
 *
 * NBT because nobody hand-edits it, and `level.nbt` rather than `level.dat` for the same reason
 * regions are `.linear`: it is not vanilla's format and should not claim to be.
 */
public data class WorldMetadata(
    val seed: Long,
    val spawnX: Int,
    val spawnY: Int,
    val spawnZ: Int,
    val timeOfDay: Long = 0L,
) {
    public fun writeTo(worldRoot: String) {
        val root =
            NbtCompound().apply {
                putInt(VERSION_KEY, FORMAT_VERSION)
                putLong("Seed", seed)
                putInt("SpawnX", spawnX)
                putInt("SpawnY", spawnY)
                putInt("SpawnZ", spawnZ)
                putLong("Time", timeOfDay)
            }
        FileIO.createDirectories(worldRoot)
        FileIO.writeBytes(pathFor(worldRoot), NbtIO.write(root))
    }

    public companion object {
        public const val FORMAT_VERSION: Int = 1

        /** Where a brand new world puts its spawn until real spawn selection exists. */
        public val DEFAULT_SPAWN: Triple<Int, Int, Int> = Triple(8, 64, 8)
        private const val VERSION_KEY = "Version"

        public fun pathFor(worldRoot: String): String = "$worldRoot/level.nbt"

        public fun read(worldRoot: String): WorldMetadata? {
            val bytes = FileIO.readBytes(pathFor(worldRoot)) ?: return null
            val root = NbtIO.read(bytes).tag as? NbtCompound ?: return null
            val version = root.getInt(VERSION_KEY)
            check(version == FORMAT_VERSION) {
                "${pathFor(worldRoot)} is format version $version, this build reads $FORMAT_VERSION"
            }
            return WorldMetadata(
                seed = root.getLong("Seed"),
                spawnX = root.getInt("SpawnX"),
                spawnY = root.getInt("SpawnY"),
                spawnZ = root.getInt("SpawnZ"),
                timeOfDay = root.getLong("Time"),
            )
        }

        /**
         * The metadata for a world that is about to be loaded, creating it on first use.
         *
         * [declaredSeed] is what `worlds.toml` asks for; the file is what the world was actually
         * generated with. **The file wins**, because the terrain on disk was made with it: changing
         * the seed of a world that already exists is an operator mistake, not an instruction, and
         * obeying it would generate mismatched terrain against the chunks already saved. Saying so
         * in the log is the useful thing to do, not silently picking either one.
         */
        public fun loadOrCreate(
            worldRoot: String,
            declaredSeed: Long?,
            defaultSpawn: Triple<Int, Int, Int>,
            logger: Logger = Logger("World"),
        ): WorldMetadata {
            val existing = read(worldRoot)
            if (existing != null) {
                if (declaredSeed != null && declaredSeed != existing.seed) {
                    logger.warn {
                        "Configured seed $declaredSeed for '$worldRoot' does not match the seed it was " +
                            "generated with (${existing.seed}); keeping the generated one. Delete the " +
                            "world directory to regenerate it with the configured seed."
                    }
                }
                return existing
            }
            val seed = declaredSeed ?: Random.nextLong()
            if (declaredSeed == null) logger.info { "Generating '$worldRoot' with a random seed: $seed." }
            val (x, y, z) = defaultSpawn
            return WorldMetadata(seed, x, y, z).also { it.writeTo(worldRoot) }
        }
    }
}
