package org.basaltmc.server.runtime.config

import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO

/** The settings that apply to one world, after inheritance has been resolved. */
public data class WorldSettings(
    public val difficulty: String,
    public val viewDistance: Int,
    public val saveIntervalSeconds: Int,
    /**
     * Whether the End's exit portal is open from the start.
     *
     * A divergence, and off by default: vanilla only opens it when the dragon dies, and there is no
     * dragon in this build yet, so an End nobody can leave is what parity actually means today.
     */
    public val openEndExitPortal: Boolean = false,
) {
    public companion object {
        public val VANILLA: WorldSettings =
            WorldSettings(
                difficulty = "easy",
                viewDistance = 10,
                saveIntervalSeconds = 30,
                openEndExitPortal = false,
            )

        /** Reads whichever of the three keys [table] declares, leaving the rest at [inherited]. */
        public fun from(
            table: TomlValue.Table?,
            inherited: WorldSettings,
        ): WorldSettings =
            WorldSettings(
                difficulty = table?.stringOrNull("difficulty") ?: inherited.difficulty,
                viewDistance = table?.intOrNull("view-distance")?.toInt() ?: inherited.viewDistance,
                saveIntervalSeconds =
                    table?.intOrNull("save-interval-seconds")?.toInt()?.coerceAtLeast(0)
                        ?: inherited.saveIntervalSeconds,
                openEndExitPortal = table?.boolOrNull("open-end-exit-portal") ?: inherited.openEndExitPortal,
            )
    }
}

/**
 * How a world's terrain is made.
 *
 * [AMPLIFIED] is declared but not implemented: vanilla builds it from a second set of density
 * functions under `overworld_amplified` whose splines this build does not port, and this project's
 * own rule is that a parity gap is declared rather than approximated. Selecting it fails at
 * startup with that explanation instead of quietly generating normal terrain under its name.
 */
public enum class WorldType {
    NORMAL,
    FLAT,
    VOID,
    AMPLIFIED,
    ;

    public val isImplemented: Boolean get() = this != AMPLIFIED

    public companion object {
        public fun byName(name: String): WorldType? = entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
    }
}

/** The vanilla dimension whose rules and generator a declared world uses. */
public enum class WorldDimension(
    public val configName: String,
    public val registryPath: String,
    public val registryIndex: Int,
    public val seaLevel: Int,
    public val chunkHeight: ChunkHeight,
    public val hasSkylight: Boolean,
    public val hasCeiling: Boolean,
    /** parity: `DimensionType.coordinateScale`, the 1:8 that makes a Nether portal land where it does. */
    public val coordinateScale: Double,
    /**
     * parity: `gameplay/nether_portal_spawns_piglin`, which the overworld's own dimension type
     * turns on and every other one leaves off.
     */
    public val spawnsPortalPiglins: Boolean = false,
) {
    OVERWORLD("overworld", "overworld", 0, 63, ChunkHeight.OVERWORLD, true, false, 1.0, spawnsPortalPiglins = true),
    NETHER("nether", "the_nether", 1, 32, ChunkHeight.NETHER, false, true, 8.0),
    END("end", "the_end", 2, 0, ChunkHeight.END, true, false, 1.0),
    ;

    /** parity: `Level.canHaveWeather`, which is why it never rains in the Nether or the End. */
    public val canHaveWeather: Boolean get() = hasSkylight && !hasCeiling && this != END

    /**
     * Whether time runs here at all.
     *
     * parity: `dimension_type.default_clock`. The Overworld and the End name a world clock in 26.2
     * and the Nether names none, which is why its sky never moves however long anyone stays in it.
     */
    public val hasClock: Boolean get() = this != NETHER

    public val registryKey: String get() = "minecraft:$registryPath"

    override fun toString(): String = configName

    public companion object {
        public fun byName(name: String): WorldDimension? =
            entries.firstOrNull {
                it.configName.equals(name, ignoreCase = true) ||
                    it.registryPath.equals(name, ignoreCase = true) ||
                    it.registryKey.equals(name, ignoreCase = true)
            }
    }
}

/**
 * One world as `worlds.toml` declares it. [seed] is null when the operator left it to chance.
 *
 * [linkedTo] is what makes several sets of dimensions possible: a Nether or an End names the
 * Overworld whose portals lead to it, so `skyblock_nether` belongs to `skyblock` and not to
 * whichever Nether happened to be declared first. Null on an Overworld, and on a secondary
 * dimension that left it out, which then belongs to the default world.
 */
public data class WorldDeclaration(
    public val name: String,
    public val dimension: WorldDimension,
    public val type: WorldType,
    public val seed: Long?,
    public val isDefault: Boolean,
    public val linkedTo: String? = null,
)

/**
 * `config/worlds.toml`: which worlds to load, plus the settings they inherit.
 *
 * Resolution order for a setting, strongest first:
 *
 * ```
 * worlds/<name>/settings.toml  ->  [defaults] here  ->  WorldSettings.VANILLA
 * ```
 */
public class WorldsConfig(
    public val worlds: List<WorldDeclaration>,
    private val defaults: WorldSettings,
    private val worldsRoot: String,
) {
    public val default: WorldDeclaration
        get() = worlds.firstOrNull { it.isDefault } ?: worlds.first()

    /** The overworld [world] belongs to: itself, what it names, or the default world. */
    public fun overworldOf(world: WorldDeclaration): WorldDeclaration {
        if (world.dimension == WorldDimension.OVERWORLD) return world
        val name = world.linkedTo ?: default.name
        return worlds.firstOrNull { it.name == name } ?: default
    }

    /**
     * The world a portal out of [world] leads to, or null when nothing is declared for it.
     *
     * Travel is always between an overworld and one of its own secondary dimensions, which is why
     * a portal in a Nether comes back to the overworld that Nether belongs to rather than to
     * whichever one is the default.
     */
    public fun portalTarget(
        world: WorldDeclaration,
        dimension: WorldDimension,
    ): WorldDeclaration? {
        val overworld = overworldOf(world)
        if (dimension == WorldDimension.OVERWORLD) return overworld
        return worlds.firstOrNull {
            it.dimension == dimension && (it.linkedTo ?: default.name) == overworld.name
        }
    }

    /**
     * The settings for [world], generating its `settings.toml` if the world directory already
     * exists and does not have one yet.
     *
     * Not generated for a world that has never been created: writing a settings file into an empty
     * directory would make the directory exist, and an existing directory is how "this world was
     * already created" is decided elsewhere.
     */
    public fun settingsFor(world: WorldDeclaration): WorldSettings {
        val path = "$worldsRoot/${world.name}/settings.toml"
        val text = FileIO.readText(path)
        if (text == null) {
            if (FileIO.directoryExists("$worldsRoot/${world.name}")) FileIO.writeText(path, WORLD_SETTINGS_DEFAULT_TOML)
            return defaults
        }
        return WorldSettings.from(Toml.parse(text), defaults)
    }

    public companion object {
        public const val FILE_NAME: String = "worlds.toml"

        /**
         * Refuses a set of links that cannot mean one thing: an unknown target, a target that is
         * not an overworld, or two worlds of the same dimension claiming the same one.
         *
         * Refusing rather than picking is the point. A server that silently sent every portal to
         * one of two Nethers would be the same guessing game Paper leaves to a plugin.
         */
        private fun validateLinks(
            worlds: List<WorldDeclaration>,
            directory: ConfigDirectory,
        ) {
            val byName = worlds.associateBy { it.name }
            val defaultWorld = worlds.firstOrNull { it.isDefault } ?: worlds.first()
            val claimed = mutableMapOf<Pair<String, WorldDimension>, String>()
            for (world in worlds) {
                if (world.dimension == WorldDimension.OVERWORLD) continue
                val targetName = world.linkedTo ?: defaultWorld.name
                val target =
                    byName[targetName]
                        ?: throw TomlFormatException(
                            "World '${world.name}' is linked to '$targetName', which " +
                                "${directory.pathOf(FILE_NAME)} does not declare.",
                        )
                if (target.dimension != WorldDimension.OVERWORLD) {
                    throw TomlFormatException(
                        "World '${world.name}' is linked to '$targetName', which is a " +
                            "${target.dimension.configName} rather than an overworld.",
                    )
                }
                val already = claimed.put(targetName to world.dimension, world.name)
                if (already != null) {
                    throw TomlFormatException(
                        "Worlds '$already' and '${world.name}' are both the " +
                            "${world.dimension.configName} of '$targetName'. A world has one of each.",
                    )
                }
            }
        }

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            worldsRoot: String = "worlds",
            logger: Logger = Logger("Config"),
        ): WorldsConfig {
            val root = directory.load(FILE_NAME, WORLDS_DEFAULT_TOML)
            val defaults = WorldSettings.from(root.tableOrNull("defaults"), WorldSettings.VANILLA)
            val declared = root.tableOrNull("worlds")

            val worlds =
                declared
                    ?.keys()
                    ?.mapNotNull { name ->
                        val table = declared.tableOrNull(name) ?: return@mapNotNull null
                        val typeName = table.stringOrNull("type") ?: "normal"
                        val type =
                            WorldType.byName(typeName)
                                ?: throw TomlFormatException(
                                    "World '$name' declares an unknown type '$typeName'. " +
                                        "Known types: ${WorldType.entries.joinToString { it.name.lowercase() }}.",
                                )
                        if (!type.isImplemented) {
                            throw TomlFormatException(
                                "World '$name' declares type '$typeName', which is not implemented yet. " +
                                    "It needs vanilla's amplified density functions, which this build does " +
                                    "not port; use \"normal\" until it does.",
                            )
                        }
                        val dimensionName = table.stringOrNull("dimension") ?: "overworld"
                        val dimension =
                            WorldDimension.byName(dimensionName)
                                ?: throw TomlFormatException(
                                    "World '$name' declares an unknown dimension '$dimensionName'. " +
                                        "Known dimensions: ${WorldDimension.entries.joinToString { it.configName }}.",
                                )
                        val linkedTo = table.stringOrNull("linked_to")
                        if (linkedTo != null && dimension == WorldDimension.OVERWORLD) {
                            throw TomlFormatException(
                                "World '$name' is an overworld and declares linked_to = \"$linkedTo\". " +
                                    "Only a nether or an end names the overworld its portals lead to.",
                            )
                        }
                        WorldDeclaration(
                            name = name,
                            dimension = dimension,
                            type = type,
                            seed = table.intOrNull("seed"),
                            isDefault = table.boolOrNull("default") ?: false,
                            linkedTo = linkedTo,
                        )
                    }.orEmpty()

            if (worlds.isEmpty()) {
                throw TomlFormatException(
                    "${directory.pathOf(FILE_NAME)} declares no worlds: a server needs at least one.",
                )
            }
            if (worlds.count { it.isDefault } > 1) {
                throw TomlFormatException(
                    "${directory.pathOf(FILE_NAME)} marks more than one world as the default.",
                )
            }
            if (worlds.none { it.isDefault }) {
                logger.warn {
                    "No world is marked as the default in ${directory.pathOf(
                        FILE_NAME,
                    )}; using '${worlds.first().name}'."
                }
            }
            validateLinks(worlds, directory)
            return WorldsConfig(worlds, defaults, worldsRoot)
        }
    }
}
