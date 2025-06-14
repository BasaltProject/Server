package org.basaltmc.server.runtime.config

import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private fun quiet() = Logger("WorldsConfigTest").apply { minLevel = LogLevel.ERROR }

private fun directory(name: String) = ConfigDirectory("build/test-data/worlds_$name", quiet())

private fun write(
    directory: ConfigDirectory,
    contents: String,
) {
    FileIO.createDirectories(directory.root)
    FileIO.writeText(directory.pathOf(WorldsConfig.FILE_NAME), contents)
}

class WorldsConfigTest {
    @Test
    fun theGeneratedDefaultDeclaresOneDefaultWorld() {
        val directory = directory("generated")
        platform.posix.remove(directory.pathOf(WorldsConfig.FILE_NAME))

        val config =
            WorldsConfig.load(
                directory,
                worldsRoot = "build/test-data/worlds_generated_root",
                logger = quiet(),
            )

        assertEquals("world", config.default.name)
        assertTrue(config.default.isDefault)
        // Left out on purpose in the generated file: a world with no declared seed gets a random
        // one the first time it is created.
        assertNull(config.default.seed)
    }

    @Test
    fun aWorldInheritsTheDefaultsAndOverridesOnlyWhatItDeclares() {
        val directory = directory("inheritance")
        val worldsRoot = "build/test-data/worlds_inheritance_root"
        write(
            directory,
            """
            [worlds.world]
            default = true

            [defaults]
            difficulty = "hard"
            view-distance = 6
            save-interval-seconds = 45
            """.trimIndent(),
        )
        val config = WorldsConfig.load(directory, worldsRoot, quiet())

        // No settings.toml yet: everything comes from [defaults].
        FileIO.createDirectories("$worldsRoot/world")
        platform.posix.remove("$worldsRoot/world/settings.toml")
        assertEquals(WorldSettings("hard", 6, 45), config.settingsFor(config.default))

        // Now the world overrides one of the three, and only that one changes.
        FileIO.writeText("$worldsRoot/world/settings.toml", "view-distance = 2\n")
        assertEquals(WorldSettings("hard", 2, 45), config.settingsFor(config.default))
        platform.posix.remove("$worldsRoot/world/settings.toml")
    }

    @Test
    fun anExistingWorldGetsASettingsFileGeneratedInsideIt() {
        val directory = directory("generated_settings")
        val worldsRoot = "build/test-data/worlds_generated_settings_root"
        write(directory, "[worlds.world]\ndefault = true\n")
        val config = WorldsConfig.load(directory, worldsRoot, quiet())

        FileIO.createDirectories("$worldsRoot/world")
        platform.posix.remove("$worldsRoot/world/settings.toml")
        config.settingsFor(config.default)

        // Inside the world directory on purpose: it is copied, moved and deleted with the world.
        assertTrue(FileIO.exists("$worldsRoot/world/settings.toml"))
        platform.posix.remove("$worldsRoot/world/settings.toml")
    }

    @Test
    fun aFileWithNoWorldsOrTwoDefaultsIsRejected() {
        val empty = directory("empty")
        write(empty, "[defaults]\ndifficulty = \"easy\"\n")
        assertFailsWith<TomlFormatException> { WorldsConfig.load(empty, "build/test-data/x", quiet()) }

        val twoDefaults = directory("two_defaults")
        write(twoDefaults, "[worlds.a]\ndefault = true\n\n[worlds.b]\ndefault = true\n")
        assertFailsWith<TomlFormatException> { WorldsConfig.load(twoDefaults, "build/test-data/x", quiet()) }
    }

    @Test
    fun dimensionsResolveAliasesAndRejectUnknownNames() {
        val directory = directory("dimensions")
        write(
            directory,
            """
            [worlds.overworld]
            default = true
            dimension = "minecraft:overworld"

            [worlds.nether]
            dimension = "the_nether"

            [worlds.end]
            dimension = "end"
            """.trimIndent(),
        )
        val config = WorldsConfig.load(directory, "build/test-data/x", quiet())

        assertEquals(
            listOf(WorldDimension.OVERWORLD, WorldDimension.NETHER, WorldDimension.END),
            config.worlds.map { it.dimension },
        )

        val invalid = directory("invalid_dimension")
        write(invalid, "[worlds.world]\ndefault = true\ndimension = \"moon\"\n")
        assertFailsWith<TomlFormatException> { WorldsConfig.load(invalid, "build/test-data/x", quiet()) }
    }

    @Test
    fun removingAWorldFromTheFileLeavesItsDataAlone() {
        // The lifecycle rule: this file declares what loads, it never deletes anything.
        val directory = directory("declaration")
        val worldsRoot = "build/test-data/worlds_declaration_root"
        FileIO.createDirectories("$worldsRoot/parked")
        FileIO.writeText("$worldsRoot/parked/marker", "still here")

        write(directory, "[worlds.world]\ndefault = true\n")
        val config = WorldsConfig.load(directory, worldsRoot, quiet())

        assertEquals(listOf("world"), config.worlds.map { it.name })
        assertEquals("still here", FileIO.readText("$worldsRoot/parked/marker"))
        platform.posix.remove("$worldsRoot/parked/marker")
    }
}

class WorldMetadataTest {
    private val root = "build/test-data/world_metadata"

    @Test
    fun aNewWorldRecordsTheSeedItWasCreatedWith() {
        platform.posix.remove(WorldMetadata.pathFor(root))
        val created =
            WorldMetadata.loadOrCreate(
                root,
                declaredSeed = 1234L,
                defaultSpawn = Triple(8, 64, 8),
                logger = quiet(),
            )

        assertEquals(1234L, created.seed)
        assertEquals(created, WorldMetadata.read(root))
        platform.posix.remove(WorldMetadata.pathFor(root))
    }

    @Test
    fun aWorldWithNoDeclaredSeedGetsARandomOneAndKeepsIt() {
        platform.posix.remove(WorldMetadata.pathFor(root))
        val first =
            WorldMetadata.loadOrCreate(
                root,
                declaredSeed = null,
                defaultSpawn = Triple(8, 64, 8),
                logger = quiet(),
            )
        val second =
            WorldMetadata.loadOrCreate(
                root,
                declaredSeed = null,
                defaultSpawn = Triple(8, 64, 8),
                logger = quiet(),
            )

        assertEquals(first.seed, second.seed)
        assertNotEquals(0L, first.seed)
        platform.posix.remove(WorldMetadata.pathFor(root))
    }

    @Test
    fun theGeneratedSeedWinsOverAConfiguredOneThatDisagrees() {
        // The point: the terrain already on disk was made with the stored seed. Obeying a changed
        // configuration would generate mismatched chunks next to it.
        platform.posix.remove(WorldMetadata.pathFor(root))
        WorldMetadata.loadOrCreate(root, declaredSeed = 111L, defaultSpawn = Triple(8, 64, 8), logger = quiet())

        val reloaded =
            WorldMetadata.loadOrCreate(
                root,
                declaredSeed = 999L,
                defaultSpawn = Triple(8, 64, 8),
                logger = quiet(),
            )

        assertEquals(111L, reloaded.seed)
        platform.posix.remove(WorldMetadata.pathFor(root))
    }
}

class WorldLevelKeyTest {
    private fun world(
        name: String,
        isDefault: Boolean,
        dimension: WorldDimension = WorldDimension.OVERWORLD,
    ) = LoadedWorld(
        declaration =
            WorldDeclaration(name, dimension, WorldType.VOID, seed = 0L, isDefault = isDefault),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 8, 64, 8),
        worldsRoot = "build/test-data/level_keys",
        logger = quiet(),
        generationWorkers = 1,
    )

    @Test
    fun theDefaultWorldKeepsVanillasOwnLevelKey() {
        // So a single-world setup looks exactly as it did, the client's debug screen included.
        assertEquals("minecraft:overworld", world("world", isDefault = true).levelKey)
    }

    @Test
    fun aDefaultNetherOrEndWorldUsesTheVanillaLevelKey() {
        assertEquals("minecraft:the_nether", world("nether", true, WorldDimension.NETHER).levelKey)
        assertEquals("minecraft:the_end", world("end", true, WorldDimension.END).levelKey)
    }

    @Test
    fun everyOtherWorldGetsItsOwnKey() {
        // The load-bearing property of world travel: the client only throws away its chunk cache
        // when the level key changes, so two worlds sharing one key would leave a player looking at
        // the terrain of the world they just left.
        assertEquals("basalt:lobby", world("lobby", isDefault = false).levelKey)
        assertNotEquals(world("lobby", false).levelKey, world("arena", false).levelKey)
    }

    @Test
    fun aNameThatIsNotAValidResourceLocationIsSanitised() {
        // Resource locations are lowercase and a restricted alphabet; a world name is neither.
        assertEquals("basalt:my_world_2", world("My World 2", isDefault = false).levelKey)
    }

    // --- Linked dimensions -------------------------------------------------------------------

    @Test
    fun theGeneratedDefaultLinksTheTwoSecondaryDimensionsToTheDefaultWorld() {
        val directory = directory("generated_links")
        platform.posix.remove(directory.pathOf(WorldsConfig.FILE_NAME))

        val config =
            WorldsConfig.load(directory, worldsRoot = "build/test-data/worlds_generated_links_root", logger = quiet())

        val nether = config.worlds.single { it.dimension == WorldDimension.NETHER }
        val end = config.worlds.single { it.dimension == WorldDimension.END }
        assertEquals("world", nether.linkedTo)
        assertEquals("world", end.linkedTo)
        assertEquals(nether, config.portalTarget(config.default, WorldDimension.NETHER))
        assertEquals(end, config.portalTarget(config.default, WorldDimension.END))
        assertEquals(config.default, config.portalTarget(nether, WorldDimension.OVERWORLD))
    }

    /** The point of the link: two overworlds, each with a Nether of its own. */
    @Test
    fun eachOverworldKeepsItsOwnDimensions() {
        val directory = directory("two_sets")
        write(
            directory,
            """
            [worlds.world]
            default = true
            [worlds.world_nether]
            dimension = "nether"
            linked_to = "world"
            [worlds.skyblock]
            [worlds.skyblock_nether]
            dimension = "nether"
            linked_to = "skyblock"
            """.trimIndent(),
        )

        val config = WorldsConfig.load(directory, "build/test-data/worlds_two_sets_root", quiet())
        val skyblock = config.worlds.single { it.name == "skyblock" }
        val skyblockNether = config.worlds.single { it.name == "skyblock_nether" }

        assertEquals(skyblockNether, config.portalTarget(skyblock, WorldDimension.NETHER))
        assertEquals(skyblock, config.portalTarget(skyblockNether, WorldDimension.OVERWORLD))
        assertEquals("world_nether", config.portalTarget(config.default, WorldDimension.NETHER)?.name)
    }

    @Test
    fun aSecondaryDimensionWithNoLinkBelongsToTheDefaultWorld() {
        val directory = directory("implicit_link")
        write(
            directory,
            """
            [worlds.world]
            default = true
            [worlds.nether]
            dimension = "nether"
            """.trimIndent(),
        )

        val config = WorldsConfig.load(directory, "build/test-data/worlds_implicit_link_root", quiet())

        assertEquals("nether", config.portalTarget(config.default, WorldDimension.NETHER)?.name)
    }

    @Test
    fun anOverworldMayNotClaimALink() {
        val directory = directory("overworld_link")
        write(
            directory,
            """
            [worlds.world]
            default = true
            linked_to = "somewhere"
            """.trimIndent(),
        )

        val failure =
            assertFailsWith<TomlFormatException> {
                WorldsConfig.load(directory, "build/test-data/worlds_overworld_link_root", quiet())
            }
        assertTrue("linked_to" in failure.message.orEmpty(), failure.message.orEmpty())
    }

    @Test
    fun aLinkToSomethingThatIsNotThereStopsTheServer() {
        val directory = directory("missing_link")
        write(
            directory,
            """
            [worlds.world]
            default = true
            [worlds.world_nether]
            dimension = "nether"
            linked_to = "nowhere"
            """.trimIndent(),
        )

        val failure =
            assertFailsWith<TomlFormatException> {
                WorldsConfig.load(directory, "build/test-data/worlds_missing_link_root", quiet())
            }
        assertTrue("nowhere" in failure.message.orEmpty(), failure.message.orEmpty())
    }

    @Test
    fun aLinkToAnotherNetherStopsTheServer() {
        val directory = directory("nether_link")
        write(
            directory,
            """
            [worlds.world]
            default = true
            [worlds.world_nether]
            dimension = "nether"
            [worlds.deeper]
            dimension = "end"
            linked_to = "world_nether"
            """.trimIndent(),
        )

        assertFailsWith<TomlFormatException> {
            WorldsConfig.load(directory, "build/test-data/worlds_nether_link_root", quiet())
        }
    }

    /** One overworld has one Nether: two claiming it is a question nobody can answer for the server. */
    @Test
    fun twoNethersForOneOverworldStopTheServer() {
        val directory = directory("two_nethers")
        write(
            directory,
            """
            [worlds.world]
            default = true
            [worlds.a_nether]
            dimension = "nether"
            linked_to = "world"
            [worlds.b_nether]
            dimension = "nether"
            linked_to = "world"
            """.trimIndent(),
        )

        val failure =
            assertFailsWith<TomlFormatException> {
                WorldsConfig.load(directory, "build/test-data/worlds_two_nethers_root", quiet())
            }
        assertTrue("a_nether" in failure.message.orEmpty(), failure.message.orEmpty())
    }
}
