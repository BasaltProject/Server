package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO
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
    /** Ticks since the world was created, which is what `/time query gametime` answers with. */
    val gameTime: Long = 0L,
    /** The clock's rate and paused flag, as `/time rate` and `/time pause` leave them. */
    val clockRate: Float = 1f,
    val clockPaused: Boolean = false,
    val raining: Boolean = false,
    val rainTime: Int = 0,
    val thundering: Boolean = false,
    val thunderTime: Int = 0,
    val clearWeatherTime: Int = 0,
    val gameRules: GameRuleValues = GameRuleValues(),
    /**
     * What `/difficulty` and `/defaultgamemode` last set, or null while nobody has: the world file
     * then follows `worlds.toml` and `server.toml`.
     *
     * parity: vanilla stores both in `level.dat`, and that file wins over `server.properties` for
     * exactly this reason: an in-game change has to outlive a restart without editing configuration
     * the operator owns.
     */
    val difficulty: String? = null,
    val difficultyLocked: Boolean = false,
    val defaultGamemode: String? = null,
    /** parity: `WanderingTraderData`, persisted independently of the spawner's 1,200-tick counter. */
    val wanderingTraderSpawnDelay: Int = 24_000,
    val wanderingTraderSpawnChance: Int = 25,
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
                putLong("GameTime", gameTime)
                putFloat("ClockRate", clockRate)
                putBoolean("ClockPaused", clockPaused)
                putBoolean("Raining", raining)
                putInt("RainTime", rainTime)
                putBoolean("Thundering", thundering)
                putInt("ThunderTime", thunderTime)
                putInt("ClearWeatherTime", clearWeatherTime)
                difficulty?.let { putString("Difficulty", it) }
                putBoolean("DifficultyLocked", difficultyLocked)
                defaultGamemode?.let { putString("DefaultGamemode", it) }
                this["WanderingTrader"] =
                    NbtCompound().apply {
                        putInt("spawn_delay", wanderingTraderSpawnDelay)
                        putInt("spawn_chance", wanderingTraderSpawnChance)
                    }
                gameRules.writeTo(this)
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
            val wanderingTrader = root.getCompound("WanderingTrader")
            return WorldMetadata(
                seed = root.getLong("Seed"),
                spawnX = root.getInt("SpawnX"),
                spawnY = root.getInt("SpawnY"),
                spawnZ = root.getInt("SpawnZ"),
                timeOfDay = root.getLong("Time"),
                gameTime = root.getLong("GameTime"),
                clockRate = if ("ClockRate" in root.keys) root.getFloat("ClockRate") else 1f,
                clockPaused = root.getBoolean("ClockPaused"),
                raining = root.getBoolean("Raining"),
                rainTime = root.getInt("RainTime"),
                thundering = root.getBoolean("Thundering"),
                thunderTime = root.getInt("ThunderTime"),
                clearWeatherTime = root.getInt("ClearWeatherTime"),
                gameRules = GameRuleValues.readFrom(root),
                difficulty = if ("Difficulty" in root.keys) root.getString("Difficulty") else null,
                difficultyLocked = root.getBoolean("DifficultyLocked"),
                defaultGamemode = if ("DefaultGamemode" in root.keys) root.getString("DefaultGamemode") else null,
                wanderingTraderSpawnDelay = wanderingTrader.getInt("spawn_delay", 24_000),
                wanderingTraderSpawnChance = wanderingTrader.getInt("spawn_chance", 25),
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
