package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the piece fill, the template pool and the pool JSON. */
class PoolAndPieceOracle {
    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun stoneChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..40) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 0..48) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (!Blocks.isAir(id) && id != Blocks.STONE.stateId) placed++
                }
            }
        }
        return checksum to placed
    }

    /** Every orientation, so the coordinate swap and the state rotation both show. */
    private fun reportFill(
        name: String,
        orientation: StructureDirection?,
    ) {
        val chunk = stoneChunk()
        val box = StructureBoundingBox(2, 4, 3, 13, 20, 12)
        val clip = StructureBoundingBox(0, 0, 0, 15, 48, 15)
        val fill = StructurePieceFill(box, orientation, chunk, clip)
        val random = WorldgenRandom(LegacyRandom(4242L))
        fill.generateBox(0, 0, 0, 8, 6, 7, state("oak_planks"), Blocks.AIR.stateId, false)
        fill.generateMaybeBox(
            random,
            0.4f,
            1,
            7,
            1,
            7,
            9,
            6,
            state("cobblestone"),
            state("mossy_cobblestone"),
            skipAir = true,
            onlyInside = false,
        )
        fill.generateUpperHalfSphere(0, 10, 0, 8, 14, 7, state("oak_log"), false)
        // A stair carries facing and half, so the orientation's state transform is visible.
        fill.place(Blocks.stateIdOf(Identifier.vanilla("oak_stairs"), mapOf("facing" to "east")) ?: 0, 3, 2, 4)
        fill.maybePlace(random, 0.5f, 4, 3, 5, state("chest"))
        fill.placeEntity(Identifier.vanilla("chest_minecart"), 5, 4, 6, NbtCompound())
        val (checksum, placed) = fold(chunk)
        println(
            "ORACLE $name placed=$placed entities=${chunk.generatedEntities().size} " +
                "interior=${fill.isInterior(3, 2, 4)} at=${fill.blockAt(3, 2, 4)} " +
                "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
        )
    }

    private val poolJson =
        """
        {
          "fallback": "minecraft:test/fallback",
          "elements": [
            {
              "weight": 2,
              "element": {
                "element_type": "minecraft:legacy_single_pool_element",
                "location": "minecraft:test/one",
                "projection": "rigid",
                "processors": {
                  "processors": [
                    {
                      "processor_type": "minecraft:rule",
                      "rules": [
                        {
                          "input_predicate": {
                            "predicate_type": "minecraft:random_block_match",
                            "block": "minecraft:cobblestone",
                            "probability": 0.25
                          },
                          "location_predicate": { "predicate_type": "minecraft:always_true" },
                          "output_state": { "Name": "minecraft:mossy_cobblestone" }
                        }
                      ]
                    },
                    { "processor_type": "minecraft:block_rot", "integrity": 0.8 },
                    {
                      "processor_type": "minecraft:capped",
                      "limit": 3,
                      "delegate": {
                        "rules": [
                          {
                            "input_predicate": {
                              "predicate_type": "minecraft:block_match",
                              "block": "minecraft:oak_planks"
                            },
                            "output_state": {
                              "Name": "minecraft:oak_stairs",
                              "Properties": { "facing": "west", "half": "top" }
                            },
                            "block_entity_modifier": {
                              "type": "minecraft:append_loot",
                              "loot_table": "minecraft:chests/village/village_house"
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            },
            {
              "weight": 1,
              "element": {
                "element_type": "minecraft:single_pool_element",
                "location": "minecraft:test/two",
                "projection": "terrain_matching"
              }
            },
            {
              "weight": 1,
              "element": {
                "element_type": "minecraft:feature_pool_element",
                "feature": "minecraft:oak",
                "projection": "rigid"
              }
            },
            { "weight": 1, "element": { "element_type": "minecraft:empty_pool_element", "projection": "rigid" } }
          ]
        }
        """.trimIndent()

    private fun describe(element: StructurePoolElement): String =
        when (element) {
            is EmptyPoolElement -> "empty"
            is LegacySinglePoolElement -> "single/${element.location}/${element.ignoreAir}"
            is FeaturePoolElement -> "feature/${element.feature}"
            is ListPoolElement -> "list/${element.elements.size}"
            else -> "other"
        } + "/${element.projection}/${element.groundLevelDelta}"

    @Test
    fun dump() {
        reportFill("fillFree", null)
        for (orientation in listOf(
            StructureDirection.NORTH,
            StructureDirection.SOUTH,
            StructureDirection.WEST,
            StructureDirection.EAST,
        )) {
            reportFill("fill${orientation.name}", orientation)
        }

        val pool = VillagePoolJson.parsePool(poolJson, resolveProcessors = { null })
        val random = WorldgenRandom(XoroshiroRandom(9001L))
        val shuffled = pool.getShuffledTemplates(random).joinToString(",", transform = ::describe)
        val drawn = (0 until 8).joinToString(",") { describe(pool.getRandomTemplate(random)) }
        println("ORACLE pool fallback=${pool.fallback} size=${pool.size()} shuffled=$shuffled drawn=$drawn")

        val processors =
            pool
                .getShuffledTemplates(WorldgenRandom(XoroshiroRandom(1L)))
                .filterIsInstance<LegacySinglePoolElement>()
                .first { it.location == "minecraft:test/one" }
                .processors
        println(
            "ORACLE processors rules=${processors.rules.size} capped=${processors.capped.size} " +
                "cappedLimit=${processors.capped.firstOrNull()?.limit} " +
                "output=${processors.rules.firstOrNull()?.outputStateId} " +
                "loot=${processors.capped.firstOrNull()?.delegate?.firstOrNull()?.appendLootTable}",
        )

        val rotations = StructureRotations.shuffled(WorldgenRandom(XoroshiroRandom(77L))).joinToString(",") { it.name }
        val single = StructureRotations.random(WorldgenRandom(XoroshiroRandom(77L))).name
        println("ORACLE rotations shuffled=$rotations random=$single")
    }
}
