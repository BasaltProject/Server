package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.decoration.end.EndRawDecoration
import org.basaltmc.server.world.terrain.decoration.end.EndSurfaceStructuresDecoration
import org.basaltmc.server.world.terrain.decoration.end.EndVegetalDecoration
import org.basaltmc.server.world.terrain.decoration.nether.NetherLocalDecoration
import org.basaltmc.server.world.terrain.decoration.nether.NetherOreDecoration
import org.basaltmc.server.world.terrain.decoration.nether.NetherSurfaceStructuresDecoration
import org.basaltmc.server.world.terrain.decoration.nether.NetherUndergroundDecoration
import org.basaltmc.server.world.terrain.decoration.nether.NetherVegetalDecoration
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.FallenTreeFeature
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * Every vanilla placed feature, runnable at one position by its own id: what `/place feature` needs,
 * since the decoration pipeline invokes a feature per step over a whole chunk and never "this one,
 * here".
 *
 * Names that differ only in their placement modifiers (`oak` and `oak_checked`) run the same body,
 * because the modifiers decide where generation puts one and the command is told where instead.
 */
public object PlacedFeatures {
    /** One feature at chunk-local [localX]/[localZ] and world [y]. */
    public fun interface Placement {
        public fun place(
            chunk: Chunk,
            random: WorldgenRandom,
            localX: Int,
            y: Int,
            localZ: Int,
        )
    }

    private fun tree(config: TreeConfiguration): Placement =
        Placement { chunk, random, x, y, z -> TreeFeature.place(chunk, random, x, y, z, config) }

    private fun mushroom(kind: HugeMushroomFeature.Kind): Placement =
        Placement { chunk, random, x, y, z -> HugeMushroomFeature.place(chunk, random, x, y, z, kind) }

    private fun fallen(config: FallenTreeConfiguration): Placement =
        Placement { chunk, random, x, y, z -> FallenTreeFeature.place(chunk, random, x, y, z, config) }

    private val TREES: Map<String, TreeConfiguration> =
        mapOf(
            "oak" to TreeConfigs.OAK,
            "oak_checked" to TreeConfigs.OAK,
            "oak_leaf_litter" to TreeConfigs.OAK_LEAF_LITTER,
            "trees_swamp" to TreeConfigs.SWAMP_OAK,
            "birch_checked" to TreeConfigs.BIRCH,
            "birch_leaf_litter" to TreeConfigs.BIRCH_LEAF_LITTER,
            "birch_tall" to TreeConfigs.SUPER_BIRCH,
            "super_birch_bees" to TreeConfigs.SUPER_BIRCH_BEES,
            "fancy_oak_checked" to TreeConfigs.FANCY_OAK,
            "fancy_oak_leaf_litter" to TreeConfigs.FANCY_OAK_LEAF_LITTER,
            "fancy_oak_bees" to TreeConfigs.FANCY_OAK_BEES,
            "jungle_tree" to TreeConfigs.JUNGLE_TREE,
            "jungle_bush" to TreeConfigs.JUNGLE_BUSH,
            "mega_jungle_tree_checked" to TreeConfigs.MEGA_JUNGLE,
            "spruce" to TreeConfigs.SPRUCE,
            "spruce_checked" to TreeConfigs.SPRUCE,
            "spruce_on_snow" to TreeConfigs.SPRUCE,
            "mega_spruce_checked" to TreeConfigs.MEGA_SPRUCE,
            "pine" to TreeConfigs.PINE,
            "pine_checked" to TreeConfigs.PINE,
            "pine_on_snow" to TreeConfigs.PINE,
            "mega_pine_checked" to TreeConfigs.MEGA_PINE,
            "acacia" to TreeConfigs.ACACIA,
            "acacia_checked" to TreeConfigs.ACACIA,
            "dark_oak_checked" to TreeConfigs.DARK_OAK,
            "dark_oak_leaf_litter" to TreeConfigs.DARK_OAK_LEAF_LITTER,
            "pale_oak_checked" to TreeConfigs.PALE_OAK,
            "pale_oak_creaking_checked" to TreeConfigs.PALE_OAK_CREAKING,
            "mangrove_checked" to TreeConfigs.MANGROVE,
            "tall_mangrove_checked" to TreeConfigs.TALL_MANGROVE,
            "cherry_checked" to TreeConfigs.CHERRY,
            // The bee variants are the same trees with a hive decorator, and vanilla names each.
            "oak_bees_002" to TreeConfigs.OAK_BEES_002,
            "oak_bees_0002_leaf_litter" to TreeConfigs.OAK_BEES_0002_LEAF_LITTER,
            "birch_bees_002" to TreeConfigs.BIRCH_BEES_002,
            "birch_bees_0002" to TreeConfigs.BIRCH_BEES_0002,
            "birch_bees_0002_leaf_litter" to TreeConfigs.BIRCH_BEES_0002_LEAF_LITTER,
            "fancy_oak_bees_002" to TreeConfigs.FANCY_OAK_BEES_002,
            "fancy_oak_bees_0002_leaf_litter" to TreeConfigs.FANCY_OAK_BEES_0002_LEAF_LITTER,
            "super_birch_bees_0002" to TreeConfigs.SUPER_BIRCH_BEES_0002,
            "cherry_bees_005" to TreeConfigs.CHERRY_BEES_005,
        )

    private val FALLEN: Map<String, FallenTreeConfiguration> =
        mapOf(
            "fallen_oak_tree" to FallenTreeConfigs.OAK,
            "fallen_birch_tree" to FallenTreeConfigs.BIRCH,
            "fallen_super_birch_tree" to FallenTreeConfigs.SUPER_BIRCH,
            "fallen_spruce_tree" to FallenTreeConfigs.SPRUCE,
            "fallen_jungle_tree" to FallenTreeConfigs.JUNGLE,
        )

    private val byId: Map<Identifier, Placement> =
        buildMap {
            for ((name, config) in TREES) put(Identifier.vanilla(name), tree(config))
            for ((name, config) in FALLEN) put(Identifier.vanilla(name), fallen(config))
            for ((name, _) in OverworldOreDecoration.PLACED_ORE_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        OverworldOreDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            for (name in OverworldOreDecoration.PLACED_DISK_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        OverworldOreDecoration.placeDiskAt(chunk, random, x, y, z, name)
                    },
                )
            }
            for (name in OverworldSpringDecoration.PLACED_SPRING_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, _, x, y, z -> OverworldSpringDecoration.placeAt(chunk, x, y, z, name) },
                )
            }
            put(
                Identifier.vanilla("patch_bush"),
                Placement { chunk, random, x, y, z ->
                    BushPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            put(
                Identifier.vanilla("patch_waterlily"),
                Placement { chunk, random, x, y, z ->
                    WaterlilyPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            put(
                Identifier.vanilla("patch_pumpkin"),
                Placement { chunk, random, x, y, z ->
                    PumpkinPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in listOf("patch_melon", "patch_melon_sparse")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        MelonPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            for (name in listOf("patch_berry_common", "patch_berry_rare", "patch_berry_bush")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        BerryPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            for (name in listOf("patch_tall_grass", "patch_tall_grass_2")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        TallGrassDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            put(
                Identifier.vanilla("grass_bonemeal"),
                Placement { chunk, random, x, y, z ->
                    GrassPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z), "patch_grass_plain")
                },
            )
            for (name in GrassPatchDecoration.PLACED_GRASS_IDS.keys) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        GrassPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in GroundPlantPatchDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        GroundPlantPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in MushroomPatchDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        MushroomPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in NetherVegetalDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        NetherVegetalDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            for (name in NetherUndergroundDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        NetherUndergroundDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            for (name in NetherSurfaceStructuresDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        NetherSurfaceStructuresDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            for (name in NetherOreDecoration.PLACED_IDS.keys) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        NetherOreDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            put(
                Identifier.vanilla("basalt_pillar"),
                Placement { chunk, random, x, y, z -> NetherLocalDecoration.placeAt(chunk, random, x, y, z) },
            )
            for (name in DoublePlantPatchDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        DoublePlantPatchDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            put(
                Identifier.vanilla("glow_lichen"),
                Placement { chunk, random, x, y, z ->
                    GlowLichenDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in AquaticDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        AquaticDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in CoralDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        CoralDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            for (name in CaveSpeleothemDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        CaveSpeleothemDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in SculkDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        SculkDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in IceSpikesDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        IceSpikesDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in ForestFlowerDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        ForestFlowerDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            for (name in DryGrassDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        DryGrassDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            for (name in listOf("patch_dead_bush", "patch_dead_bush_2", "patch_dead_bush_badlands")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        DesertDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            put(
                Identifier.vanilla("flower_meadow"),
                Placement { chunk, random, x, y, z ->
                    MeadowDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in listOf("flower_pale_garden", "pale_garden_flowers")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        PaleGardenDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            put(
                Identifier.vanilla("flower_cherry"),
                Placement { chunk, random, x, y, z ->
                    CherryGroveDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in BambooDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        BambooDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            put(
                Identifier.vanilla("vines"),
                Placement { chunk, _, x, y, z -> JungleVineDecoration.placeAt(chunk, DecorationPos(x, y, z)) },
            )
            put(
                Identifier.vanilla("mushroom_island_vegetation"),
                Placement { chunk, random, x, y, z ->
                    MushroomFieldsDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            put(
                Identifier.vanilla("pale_moss_patch"),
                Placement { chunk, random, x, y, z ->
                    PaleMossDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in CaveLocalModificationDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        CaveLocalModificationDecoration.placeAt(chunk, random, x, y, z, name)
                    },
                )
            }
            put(
                Identifier.vanilla("end_island_decorated"),
                Placement { chunk, random, x, y, z -> EndRawDecoration.placeAt(chunk, random, x, y, z) },
            )
            for (name in FrozenOceanDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        FrozenOceanDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            put(
                Identifier.vanilla("underwater_magma"),
                Placement { chunk, _, x, y, z -> OverworldOreDecoration.placeUnderwaterMagmaAt(chunk, x, y, z) },
            )
            for (name in DefaultVegetationDecoration.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        DefaultVegetationDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            put(
                Identifier.vanilla("flower_swamp"),
                Placement { chunk, random, x, y, z ->
                    SwampDecoration.placeAt(chunk, random, DecorationPos(x, y, z))
                },
            )
            for (name in listOf("flower_plain", "flower_plains")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        PlainsDecoration.placeFlowerPlainAt(chunk, random, x, y, z)
                    },
                )
            }
            for (name in TreeDecoration.SELECTORS.keys) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        TreeDecoration.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in SmallFeatures.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        SmallFeatures.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            for (name in StandaloneFeatures.PLACED_IDS) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        StandaloneFeatures.placeAt(chunk, random, DecorationPos(x, y, z), name)
                    },
                )
            }
            // parity: the three `patch_cactus*` placed features are the same configured feature
            // with different placement modifiers, and `/place feature` supplies the position.
            for (name in listOf("patch_cactus", "patch_cactus_desert", "patch_cactus_decorated")) {
                put(
                    Identifier.vanilla(name),
                    Placement { chunk, random, x, y, z ->
                        CactusFeature.place(chunk, random, DecorationPos(x, y, z))
                    },
                )
            }
            put(Identifier.vanilla("huge_brown_mushroom"), mushroom(HugeMushroomFeature.Kind.BROWN))
            put(Identifier.vanilla("huge_red_mushroom"), mushroom(HugeMushroomFeature.Kind.RED))
            put(
                Identifier.vanilla("forest_rock"),
                Placement { chunk, random, x, y, z -> ForestRockDecoration.placeAt(chunk, random, x, y, z) },
            )
            put(
                Identifier.vanilla("desert_well"),
                Placement { chunk, random, x, y, z -> DesertWellDecoration.placeAt(chunk, random, x, y, z) },
            )
        }

    /**
     * What a feature needs beyond its chunk: the world's seed, its noise routers, its surface rules
     * and its structure templates.
     *
     * Five vanilla features cannot be run from a chunk alone (the End's spikes and chorus plants,
     * the fossils, the freeze pass), and this is how they are handed the rest. A caller that has
     * none of it gets a refusal by name rather than a half-placed feature.
     */
    public class FeatureContext(
        public val seed: Long = 0L,
        public val endRouter: EndNoiseRouter? = null,
        public val surface: SurfaceRuleEnvironment? = null,
        public val templates: ((Identifier) -> StructureTemplate?)? = null,
    )

    /** A feature that needs [FeatureContext] as well as a position. */
    public fun interface ContextualPlacement {
        public fun place(
            chunk: Chunk,
            random: WorldgenRandom,
            localX: Int,
            y: Int,
            localZ: Int,
            context: FeatureContext,
        ): Boolean
    }

    private val byIdWithContext: Map<Identifier, ContextualPlacement> =
        mapOf(
            Identifier.vanilla("end_spike") to
                ContextualPlacement { chunk, _, _, _, _, context ->
                    EndSurfaceStructuresDecoration.placeSpikesIn(chunk, context.seed)
                },
            Identifier.vanilla("chorus_plant") to
                ContextualPlacement { chunk, random, x, y, z, context ->
                    val router = context.endRouter ?: return@ContextualPlacement false
                    EndVegetalDecoration.placeAt(chunk, random, router, x, y, z)
                },
            Identifier.vanilla("freeze_top_layer") to
                ContextualPlacement { chunk, _, x, _, z, context ->
                    val surface = context.surface ?: return@ContextualPlacement false
                    SurfaceFreezingDecoration.placeAt(chunk, surface, x, z)
                    true
                },
            Identifier.vanilla("fossil_lower") to fossil(FOSSIL_LOWER),
            Identifier.vanilla("fossil_upper") to fossil(FOSSIL_UPPER),
        )

    /**
     * parity: `FossilFeature`, which picks one of its structures and buries it. The pieces are
     * structure files from the operator's own jar, so this needs the template loader.
     */
    private fun fossil(structures: List<String>): ContextualPlacement =
        ContextualPlacement { chunk, random, x, y, z, context ->
            val loader = context.templates ?: return@ContextualPlacement false
            val chosen = structures[random.nextInt(structures.size)]
            val template = loader(Identifier.vanilla(chosen)) ?: return@ContextualPlacement false
            val position = StructurePos(chunk.chunkX * 16 + (x and 15), y, chunk.chunkZ * 16 + (z and 15))
            val pivot = StructurePos(0, 0, 0)
            template.placeInto(
                chunk = chunk,
                position = position,
                rotation = StructureRotation.NONE,
                mirror = StructureMirror.NONE,
                pivot = pivot,
                clipTo = template.getBoundingBox(position, StructureRotation.NONE, pivot, StructureMirror.NONE),
                ignoreStructureBlocks = true,
            )
            true
        }

    /** Every id that needs a [FeatureContext], which is the five that cannot run from a chunk alone. */
    public val contextualIds: Set<Identifier> get() = byIdWithContext.keys

    public fun contextual(id: Identifier): ContextualPlacement? = byIdWithContext[id]

    public val ids: Set<Identifier> get() = byId.keys + byIdWithContext.keys

    public operator fun get(id: Identifier): Placement? = byId[id]
}

/** parity: `FossilFeature`'s own two structure lists, lower and upper. */
private val FOSSIL_LOWER =
    listOf(
        "fossil/spine_1",
        "fossil/spine_2",
        "fossil/spine_3",
        "fossil/spine_4",
        "fossil/skull_1",
        "fossil/skull_2",
        "fossil/skull_3",
        "fossil/skull_4",
    )

private val FOSSIL_UPPER =
    listOf(
        "fossil/spine_1",
        "fossil/spine_2",
        "fossil/spine_3",
        "fossil/spine_4",
        "fossil/skull_1",
        "fossil/skull_2",
        "fossil/skull_3",
        "fossil/skull_4",
    )
