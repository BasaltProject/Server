package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

internal data class StateKey(
    val block: Identifier,
    val properties: Map<String, String>,
)

internal val derivedStatesById: Map<Int, BlockState> by lazy {
    buildMap {
        for (path in AXIS_BLOCKS) {
            val default = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
            put(default.stateId - 1, BlockState(default.block, default.stateId - 1, mapOf("axis" to "x")))
            put(default.stateId, BlockState(default.block, default.stateId, mapOf("axis" to "y")))
            put(default.stateId + 1, BlockState(default.block, default.stateId + 1, mapOf("axis" to "z")))
        }

        for (path in AMETHYST_CLUSTER_BLOCKS) {
            val default = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
            var stateId = default.stateId - 9
            for (facing in listOf("north", "east", "south", "west", "up", "down")) {
                for (waterlogged in listOf("true", "false")) {
                    put(
                        stateId,
                        BlockState(
                            default.block,
                            stateId,
                            linkedMapOf("facing" to facing, "waterlogged" to waterlogged),
                        ),
                    )
                    stateId++
                }
            }
        }

        for (path in SPELEOTHEM_BLOCKS) {
            val default = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
            var stateId = default.stateId - 5
            for (thickness in listOf("tip_merge", "tip", "frustum", "middle", "base")) {
                for (verticalDirection in listOf("up", "down")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            stateId,
                            BlockState(
                                default.block,
                                stateId,
                                linkedMapOf(
                                    "thickness" to thickness,
                                    "vertical_direction" to verticalDirection,
                                    "waterlogged" to waterlogged,
                                ),
                            ),
                        )
                        stateId++
                    }
                }
            }
        }

        // parity: reports/blocks.json state order used by NetherFortressPieces.
        val netherBrickFence =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("nether_brick_fence"))
                ?: error("unknown block 'nether_brick_fence'")
        var fenceStateId = netherBrickFence.stateId - 31
        for (east in listOf("true", "false")) {
            for (north in listOf("true", "false")) {
                for (south in listOf("true", "false")) {
                    for (waterlogged in listOf("true", "false")) {
                        for (west in listOf("true", "false")) {
                            put(
                                fenceStateId,
                                BlockState(
                                    netherBrickFence.block,
                                    fenceStateId,
                                    linkedMapOf(
                                        "east" to east,
                                        "north" to north,
                                        "south" to south,
                                        "waterlogged" to waterlogged,
                                        "west" to west,
                                    ),
                                ),
                            )
                            fenceStateId++
                        }
                    }
                }
            }
        }

        val netherBrickStairs =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("nether_brick_stairs"))
                ?: error("unknown block 'nether_brick_stairs'")
        for ((facing, offset) in mapOf("north" to 0, "south" to 20, "west" to 40, "east" to 60)) {
            val stairsStateId = netherBrickStairs.stateId + offset
            put(
                stairsStateId,
                BlockState(
                    netherBrickStairs.block,
                    stairsStateId,
                    linkedMapOf(
                        "facing" to facing,
                        "half" to "bottom",
                        "shape" to "straight",
                        "waterlogged" to "false",
                    ),
                ),
            )
        }

        // parity: reports/blocks.json states emitted by Nether ruined portal templates and
        // BlackstoneReplaceProcessor after mirror/rotation and BlockAgeProcessor.
        val polishedBlackstoneBrickStairs =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("polished_blackstone_brick_stairs"))
                ?: error("unknown block 'polished_blackstone_brick_stairs'")
        val portalStairIds =
            mapOf(
                "north" to mapOf("top" to 22253, "bottom" to 22263),
                "south" to mapOf("top" to 22273, "bottom" to 22283),
                "west" to mapOf("top" to 22293, "bottom" to 22303),
                "east" to mapOf("top" to 22313, "bottom" to 22323),
            )
        for ((facing, halves) in portalStairIds) {
            for ((half, stateId) in halves) {
                put(
                    stateId,
                    BlockState(
                        polishedBlackstoneBrickStairs.block,
                        stateId,
                        linkedMapOf(
                            "facing" to facing,
                            "half" to half,
                            "shape" to "straight",
                            "waterlogged" to "false",
                        ),
                    ),
                )
            }
        }

        for (
        (path, states) in
        mapOf(
            "polished_blackstone_slab" to mapOf("top" to 22738, "bottom" to 22740, "double" to 22742),
            "polished_blackstone_brick_slab" to mapOf("top" to 22247, "bottom" to 22249, "double" to 22251),
        )
        ) {
            val slab = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
            for ((type, stateId) in states) {
                put(
                    stateId,
                    BlockState(
                        slab.block,
                        stateId,
                        linkedMapOf("type" to type, "waterlogged" to "false"),
                    ),
                )
            }
        }

        val chest = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chest")) ?: error("unknown block 'chest'")
        for ((facing, stateId) in mapOf("north" to 3988, "south" to 3994, "west" to 4000, "east" to 4006)) {
            put(
                stateId,
                BlockState(
                    chest.block,
                    stateId,
                    linkedMapOf("type" to "single", "facing" to facing, "waterlogged" to "false"),
                ),
            )
        }

        // parity: reports/blocks.json, including the flowing lava levels stored by giant
        // ruined portal templates rather than collapsing them to the source state.
        for (level in 0..15) {
            val stateId = Blocks.LAVA.stateId + level
            put(stateId, BlockState(Blocks.LAVA.block, stateId, mapOf("level" to level.toString())))
        }

        val vine = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("vine")) ?: error("unknown block 'vine'")
        for (stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID) {
            val enabledBits = VINE_DEFAULT_STATE_ID - stateId
            put(
                stateId,
                BlockState(
                    vine.block,
                    stateId,
                    linkedMapOf(
                        "east" to booleanProperty(enabledBits and 16 != 0),
                        "north" to booleanProperty(enabledBits and 8 != 0),
                        "south" to booleanProperty(enabledBits and 4 != 0),
                        "up" to booleanProperty(enabledBits and 2 != 0),
                        "west" to booleanProperty(enabledBits and 1 != 0),
                    ),
                ),
            )
        }

        val glowLichen =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("glow_lichen")) ?: error("unknown block 'glow_lichen'")
        for (stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID) {
            val falseBits = stateId - GLOW_LICHEN_FIRST_STATE_ID
            put(
                stateId,
                BlockState(
                    glowLichen.block,
                    stateId,
                    linkedMapOf(
                        "down" to booleanProperty(falseBits and 64 == 0),
                        "east" to booleanProperty(falseBits and 32 == 0),
                        "north" to booleanProperty(falseBits and 16 == 0),
                        "south" to booleanProperty(falseBits and 8 == 0),
                        "up" to booleanProperty(falseBits and 4 == 0),
                        "waterlogged" to booleanProperty(falseBits and 2 == 0),
                        "west" to booleanProperty(falseBits and 1 == 0),
                    ),
                ),
            )
        }

        val sculkVein =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sculk_vein")) ?: error("unknown block 'sculk_vein'")
        for (stateId in sculkVein.stateId - 127..sculkVein.stateId) {
            val falseBits = stateId - (sculkVein.stateId - 127)
            put(
                stateId,
                BlockState(
                    sculkVein.block,
                    stateId,
                    linkedMapOf(
                        "down" to booleanProperty(falseBits and 64 == 0),
                        "east" to booleanProperty(falseBits and 32 == 0),
                        "north" to booleanProperty(falseBits and 16 == 0),
                        "south" to booleanProperty(falseBits and 8 == 0),
                        "up" to booleanProperty(falseBits and 4 == 0),
                        "waterlogged" to booleanProperty(falseBits and 2 == 0),
                        "west" to booleanProperty(falseBits and 1 == 0),
                    ),
                ),
            )
        }

        val sculkSensor =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sculk_sensor")) ?: error("unknown block 'sculk_sensor'")
        var sculkSensorStateId = sculkSensor.stateId - 1
        for (power in 0..15) {
            for (phase in listOf("inactive", "active", "cooldown")) {
                for (waterlogged in listOf("true", "false")) {
                    put(
                        sculkSensorStateId,
                        BlockState(
                            sculkSensor.block,
                            sculkSensorStateId,
                            linkedMapOf(
                                "power" to power.toString(),
                                "sculk_sensor_phase" to phase,
                                "waterlogged" to waterlogged,
                            ),
                        ),
                    )
                    sculkSensorStateId++
                }
            }
        }

        val sculkCatalyst =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sculk_catalyst"))
                ?: error("unknown block 'sculk_catalyst'")
        for ((bloom, stateId) in listOf("true", "false").zip(sculkCatalyst.stateId - 1..sculkCatalyst.stateId)) {
            put(stateId, BlockState(sculkCatalyst.block, stateId, mapOf("bloom" to bloom)))
        }

        val sculkShrieker =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sculk_shrieker"))
                ?: error("unknown block 'sculk_shrieker'")
        var sculkShriekerStateId = sculkShrieker.stateId - 7
        for (canSummon in listOf("true", "false")) {
            for (shrieking in listOf("true", "false")) {
                for (waterlogged in listOf("true", "false")) {
                    put(
                        sculkShriekerStateId,
                        BlockState(
                            sculkShrieker.block,
                            sculkShriekerStateId,
                            linkedMapOf(
                                "can_summon" to canSummon,
                                "shrieking" to shrieking,
                                "waterlogged" to waterlogged,
                            ),
                        ),
                    )
                    sculkShriekerStateId++
                }
            }
        }

        val sweetBerry =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sweet_berry_bush"))
                ?: error("unknown block 'sweet_berry_bush'")
        for (age in 0..3) {
            put(
                sweetBerry.stateId + age,
                BlockState(
                    sweetBerry.block,
                    sweetBerry.stateId + age,
                    mapOf(
                        "age" to age.toString(),
                    ),
                ),
            )
        }

        val bamboo = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("bamboo")) ?: error("unknown block 'bamboo'")
        var bambooStateId = bamboo.stateId
        for (age in 0..1) {
            for (leaves in listOf("none", "small", "large")) {
                for (stage in 0..1) {
                    put(
                        bambooStateId,
                        BlockState(
                            bamboo.block,
                            bambooStateId,
                            linkedMapOf(
                                "age" to age.toString(),
                                "leaves" to leaves,
                                "stage" to stage.toString(),
                            ),
                        ),
                    )
                    bambooStateId++
                }
            }
        }

        val cocoa = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("cocoa")) ?: error("unknown block 'cocoa'")
        val facings = listOf("north", "south", "west", "east")
        for (age in 0..2) {
            for ((facingOffset, facing) in facings.withIndex()) {
                val stateId = cocoa.stateId + age * facings.size + facingOffset
                put(
                    stateId,
                    BlockState(
                        cocoa.block,
                        stateId,
                        linkedMapOf(
                            "age" to age.toString(),
                            "facing" to facing,
                        ),
                    ),
                )
            }
        }

        val paleHangingMoss =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss"))
                ?: error("unknown block 'pale_hanging_moss'")
        put(
            paleHangingMoss.stateId,
            BlockState(
                paleHangingMoss.block,
                paleHangingMoss.stateId,
                mapOf(
                    "tip" to "true",
                ),
            ),
        )
        put(
            paleHangingMoss.stateId + 1,
            BlockState(
                paleHangingMoss.block,
                paleHangingMoss.stateId + 1,
                mapOf(
                    "tip" to "false",
                ),
            ),
        )

        val creakingHeart =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart"))
                ?: error("unknown block 'creaking_heart'")
        val axes = listOf("x", "y", "z")
        val heartStates = listOf("uprooted", "dormant", "awake")
        var stateId = CREAKING_HEART_FIRST_STATE_ID
        for (axis in axes) {
            for (heartState in heartStates) {
                for (natural in listOf("true", "false")) {
                    put(
                        stateId,
                        BlockState(
                            creakingHeart.block,
                            stateId,
                            linkedMapOf("axis" to axis, "creaking_heart_state" to heartState, "natural" to natural),
                        ),
                    )
                    stateId++
                }
            }
        }

        val mangrovePropagule =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule"))
                ?: error("unknown block 'mangrove_propagule'")
        var propaguleStateId = MANGROVE_PROPAGULE_FIRST_STATE_ID
        for (age in 0..4) {
            for (hanging in listOf("true", "false")) {
                for (stage in 0..1) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            propaguleStateId,
                            BlockState(
                                mangrovePropagule.block,
                                propaguleStateId,
                                linkedMapOf(
                                    "age" to age.toString(),
                                    "hanging" to hanging,
                                    "stage" to stage.toString(),
                                    "waterlogged" to waterlogged,
                                ),
                            ),
                        )
                        propaguleStateId++
                    }
                }
            }
        }

        val leafLitter =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter")) ?: error("unknown block 'leaf_litter'")
        var leafLitterStateId = LEAF_LITTER_DEFAULT_STATE_ID
        for (facing in listOf("north", "south", "west", "east")) {
            for (segmentAmount in 1..4) {
                put(
                    leafLitterStateId,
                    BlockState(
                        leafLitter.block,
                        leafLitterStateId,
                        linkedMapOf("facing" to facing, "segment_amount" to segmentAmount.toString()),
                    ),
                )
                leafLitterStateId++
            }
        }

        val beeNest = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("bee_nest")) ?: error("unknown block 'bee_nest'")
        var beeNestStateId = BEE_NEST_DEFAULT_STATE_ID
        for (facing in listOf("north", "south", "west", "east")) {
            for (honeyLevel in 0..5) {
                put(
                    beeNestStateId,
                    BlockState(
                        beeNest.block,
                        beeNestStateId,
                        linkedMapOf("facing" to facing, "honey_level" to honeyLevel.toString()),
                    ),
                )
                beeNestStateId++
            }
        }

        val chorusPlant =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chorus_plant")) ?: error("unknown block 'chorus_plant'")
        for (stateId in CHORUS_PLANT_FIRST_STATE_ID..CHORUS_PLANT_DEFAULT_STATE_ID) {
            val falseBits = stateId - CHORUS_PLANT_FIRST_STATE_ID
            put(
                stateId,
                BlockState(
                    chorusPlant.block,
                    stateId,
                    linkedMapOf(
                        "down" to booleanProperty(falseBits and 32 == 0),
                        "east" to booleanProperty(falseBits and 16 == 0),
                        "north" to booleanProperty(falseBits and 8 == 0),
                        "south" to booleanProperty(falseBits and 4 == 0),
                        "up" to booleanProperty(falseBits and 2 == 0),
                        "west" to booleanProperty(falseBits and 1 == 0),
                    ),
                ),
            )
        }

        val chorusFlower =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chorus_flower")) ?: error("unknown block 'chorus_flower'")
        for (age in 0..5) {
            put(
                CHORUS_FLOWER_DEFAULT_STATE_ID + age,
                BlockState(
                    chorusFlower.block,
                    CHORUS_FLOWER_DEFAULT_STATE_ID + age,
                    mapOf("age" to age.toString()),
                ),
            )
        }

        for (stateId in Blocks.IRON_BARS.stateId - 31..Blocks.IRON_BARS.stateId) {
            val falseBits = stateId - (Blocks.IRON_BARS.stateId - 31)
            put(
                stateId,
                BlockState(
                    Blocks.IRON_BARS.block,
                    stateId,
                    linkedMapOf(
                        "east" to booleanProperty(falseBits and 16 == 0),
                        "north" to booleanProperty(falseBits and 8 == 0),
                        "south" to booleanProperty(falseBits and 4 == 0),
                        "waterlogged" to booleanProperty(falseBits and 2 == 0),
                        "west" to booleanProperty(falseBits and 1 == 0),
                    ),
                ),
            )
        }

        for (path in LEAF_BLOCKS) {
            val leaves = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
            var leafStateId = leaves.stateId - 27
            for (distance in 1..7) {
                for (persistent in listOf("true", "false")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            leafStateId,
                            BlockState(
                                leaves.block,
                                leafStateId,
                                linkedMapOf(
                                    "distance" to distance.toString(),
                                    "persistent" to persistent,
                                    "waterlogged" to waterlogged,
                                ),
                            ),
                        )
                        leafStateId++
                    }
                }
            }
        }

        for (path in DOUBLE_PLANT_BLOCKS) {
            val plant = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
            put(plant.stateId - 1, BlockState(plant.block, plant.stateId - 1, mapOf("half" to "upper")))
            put(plant.stateId, BlockState(plant.block, plant.stateId, mapOf("half" to "lower")))
        }

        for (defaultStateId in SNOWY_BLOCK_DEFAULT_STATE_IDS) {
            val block =
                Blocks.REGISTRY.byId(defaultStateId)?.block ?: error("unknown snowy block state '$defaultStateId'")
            put(defaultStateId - 1, BlockState(block, defaultStateId - 1, mapOf("snowy" to "true")))
            put(defaultStateId, BlockState(block, defaultStateId, mapOf("snowy" to "false")))
        }

        for (defaultStateId in CORAL_WATERLOGGED_DEFAULT_STATE_IDS) {
            val block = Blocks.REGISTRY.byId(defaultStateId)?.block ?: error("unknown coral state '$defaultStateId'")
            put(defaultStateId, BlockState(block, defaultStateId, mapOf("waterlogged" to "true")))
            put(defaultStateId + 1, BlockState(block, defaultStateId + 1, mapOf("waterlogged" to "false")))
        }

        for (defaultStateId in CORAL_WALL_FAN_DEFAULT_STATE_IDS) {
            val block =
                Blocks.REGISTRY.byId(defaultStateId)?.block ?: error("unknown wall coral state '$defaultStateId'")
            var wallCoralStateId = defaultStateId
            for (facing in listOf("north", "south", "west", "east")) {
                for (waterlogged in listOf("true", "false")) {
                    put(
                        wallCoralStateId,
                        BlockState(
                            block,
                            wallCoralStateId,
                            linkedMapOf("facing" to facing, "waterlogged" to waterlogged),
                        ),
                    )
                    wallCoralStateId++
                }
            }
        }

        for (defaultStateId in HUGE_MUSHROOM_DEFAULT_STATE_IDS) {
            val block =
                Blocks.REGISTRY.byId(defaultStateId)?.block
                    ?: error("unknown huge mushroom state '$defaultStateId'")
            var mushroomStateId = defaultStateId
            for (down in listOf("true", "false")) {
                for (east in listOf("true", "false")) {
                    for (north in listOf("true", "false")) {
                        for (south in listOf("true", "false")) {
                            for (up in listOf("true", "false")) {
                                for (west in listOf("true", "false")) {
                                    put(
                                        mushroomStateId,
                                        BlockState(
                                            block,
                                            mushroomStateId,
                                            linkedMapOf(
                                                "down" to down,
                                                "east" to east,
                                                "north" to north,
                                                "south" to south,
                                                "up" to up,
                                                "west" to west,
                                            ),
                                        ),
                                    )
                                    mushroomStateId++
                                }
                            }
                        }
                    }
                }
            }
        }

        for (path in listOf("pink_petals", "wildflowers")) {
            val flowerBed = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
            var flowerBedStateId = flowerBed.stateId
            for (facing in listOf("north", "south", "west", "east")) {
                for (flowerAmount in 1..4) {
                    put(
                        flowerBedStateId,
                        BlockState(
                            flowerBed.block,
                            flowerBedStateId,
                            linkedMapOf("facing" to facing, "flower_amount" to flowerAmount.toString()),
                        ),
                    )
                    flowerBedStateId++
                }
            }
        }

        for (age in 0..25) {
            put(
                Blocks.KELP.stateId + age,
                BlockState(Blocks.KELP.block, Blocks.KELP.stateId + age, mapOf("age" to age.toString())),
            )
        }

        for (defaultStateId in GROWING_VINE_HEAD_DEFAULT_STATE_IDS) {
            val block = Blocks.REGISTRY.byId(defaultStateId)?.block ?: error("unknown vine head '$defaultStateId'")
            for (age in 0..25) {
                put(defaultStateId + age, BlockState(block, defaultStateId + age, mapOf("age" to age.toString())))
            }
        }

        var seaPickleStateId = Blocks.SEA_PICKLE.stateId
        for (pickles in 1..4) {
            for (waterlogged in listOf("true", "false")) {
                put(
                    seaPickleStateId,
                    BlockState(
                        Blocks.SEA_PICKLE.block,
                        seaPickleStateId,
                        linkedMapOf("pickles" to pickles.toString(), "waterlogged" to waterlogged),
                    ),
                )
                seaPickleStateId++
            }
        }

        // parity: village templates need arbitrary palette properties and rotate them after
        // loading. These compact generated state-definition specs expand in the registry's
        // exact Cartesian-product order, covering every rotation result, not only states that
        // happened to appear unrotated in one NBT template.
        for (encoded in VILLAGE_BLOCK_STATE_SPECS + PORTAL_BLOCK_STATE_SPECS) {
            val fields = encoded.split('|', limit = 3)
            var stateId = fields[0].toInt()
            val block = Identifier.vanilla(fields[1])
            val propertySpecs =
                fields[2]
                    .split(';')
                    .filter(String::isNotEmpty)
                    .map { property ->
                        val separator = property.indexOf('=')
                        property.substring(0, separator) to property.substring(separator + 1).split(',')
                    }
            val properties = linkedMapOf<String, String>()

            fun addStates(propertyIndex: Int) {
                if (propertyIndex == propertySpecs.size) {
                    put(stateId, BlockState(block, stateId, properties.toMap()))
                    stateId++
                    return
                }
                val (name, values) = propertySpecs[propertyIndex]
                for (value in values) {
                    properties[name] = value
                    addStates(propertyIndex + 1)
                }
                properties.remove(name)
            }
            addStates(0)
        }
    }
}

internal val derivedStateIds: Map<StateKey, Int> by lazy {
    derivedStatesById.values.associate { StateKey(it.block, it.properties) to it.stateId }
}

private fun booleanProperty(value: Boolean): String = if (value) "true" else "false"

private val AXIS_BLOCKS: Set<String> =
    setOf(
        "oak_log",
        "spruce_log",
        "birch_log",
        "jungle_log",
        "acacia_log",
        "cherry_log",
        "dark_oak_log",
        "pale_oak_log",
        "mangrove_log",
    )
private val AMETHYST_CLUSTER_BLOCKS: List<String> =
    listOf("amethyst_cluster", "large_amethyst_bud", "medium_amethyst_bud", "small_amethyst_bud")
private val SPELEOTHEM_BLOCKS: List<String> = listOf("pointed_dripstone", "sulfur_spike")
private val LEAF_BLOCKS: List<String> =
    listOf(
        "oak_leaves",
        "spruce_leaves",
        "birch_leaves",
        "jungle_leaves",
        "acacia_leaves",
        "cherry_leaves",
        "dark_oak_leaves",
        "pale_oak_leaves",
        "mangrove_leaves",
        "azalea_leaves",
        "flowering_azalea_leaves",
    )
private val CHORUS_PLANT_FIRST_STATE_ID: Int = CHORUS_PLANT_DEFAULT_STATE_ID - 63
