package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO

/** The settings that apply to one world, after inheritance has been resolved. */
public data class WorldSettings(
    public val difficulty: String,
    public val viewDistance: Int,
    public val saveIntervalSeconds: Int,
) {
    public companion object {
        public val VANILLA: WorldSettings =
            WorldSettings(difficulty = "easy", viewDistance = 10, saveIntervalSeconds = 30)

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

/** One world as `worlds.toml` declares it. [seed] is null when the operator left it to chance. */
public data class WorldDeclaration(
    public val name: String,
    public val dimension: String,
    public val type: WorldType,
    public val seed: Long?,
    public val isDefault: Boolean,
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
            if (FileIO.exists("$worldsRoot/${world.name}")) FileIO.writeText(path, WORLD_SETTINGS_DEFAULT_TOML)
            return defaults
        }
        return WorldSettings.from(Toml.parse(text), defaults)
    }

    public companion object {
        public const val FILE_NAME: String = "worlds.toml"

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
                        WorldDeclaration(
                            name = name,
                            dimension = table.stringOrNull("dimension") ?: "overworld",
                            type = type,
                            seed = table.intOrNull("seed"),
                            isDefault = table.boolOrNull("default") ?: false,
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
            return WorldsConfig(worlds, defaults, worldsRoot)
        }
    }
}
