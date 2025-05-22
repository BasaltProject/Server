package org.basaltmc.world.structure

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs

/**
 * Synthetic JSON matching the real `template_pool`/`processor_list` schema shape (verified against
 * the real jar's own `town_centers.json`/`mossify_20_percent.json`), but with fabricated block
 * choices and `test:` namespaced names: not copied from any real Mojang datapack file, per §7.
 */
class VillagePoolJsonTest {
    private fun fakeRandom() = WorldgenRandom(XoroshiroRandom(1L))

    @Test
    fun parsesElementsWeightsAndFallback() {
        val json =
            """
            {
              "elements": [
                {
                  "element": {
                    "element_type": "minecraft:legacy_single_pool_element",
                    "location": "test:sample/piece_one",
                    "processors": {"processors": []},
                    "projection": "rigid"
                  },
                  "weight": 3
                },
                {
                  "element": {
                    "element_type": "minecraft:empty_pool_element"
                  },
                  "weight": 1
                }
              ],
              "fallback": "test:sample_fallback"
            }
            """.trimIndent()

        val pool = VillagePoolJson.parsePool(json) { null }
        assertEquals(4, pool.size())
        assertEquals("test:sample_fallback", pool.fallback)
    }

    @Test
    fun resolvesANamedProcessorListReference() {
        val json =
            """
            {
              "elements": [
                {
                  "element": {
                    "element_type": "minecraft:legacy_single_pool_element",
                    "location": "test:sample/piece",
                    "processors": "test:my_processor_list",
                    "projection": "rigid"
                  },
                  "weight": 1
                }
              ],
              "fallback": "minecraft:empty"
            }
            """.trimIndent()

        val processorListJson =
            """
            {
              "processors": [
                {
                  "processor_type": "minecraft:rule",
                  "rules": [
                    {
                      "input_predicate": {
                        "block": "minecraft:stone",
                        "predicate_type": "minecraft:random_block_match",
                        "probability": 0.5
                      },
                      "location_predicate": {
                        "predicate_type": "minecraft:always_true"
                      },
                      "output_state": {
                        "Name": "minecraft:andesite"
                      }
                    }
                  ]
                }
              ]
            }
            """.trimIndent()

        val pool =
            VillagePoolJson.parsePool(json) { name ->
                if (name ==
                    "test:my_processor_list"
                ) {
                    processorListJson
                } else {
                    null
                }
            }
        val element = assertIs<LegacySinglePoolElement>(pool.getRandomTemplate(fakeRandom()))
        assertEquals(1, element.processors.rules.size)

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        val andesiteId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("andesite"))!!.stateId
        // probability 0.5 with a fixed seed: just confirm the rule can fire and produces the right output.
        var firedOnce = false
        val random = fakeRandom()
        repeat(20) {
            if (element.processors.process(stoneId, Blocks.AIR.stateId, random) == andesiteId) firedOnce = true
        }
        assertEquals(true, firedOnce, "a 0.5-probability rule should fire at least once in 20 tries")
    }

    @Test
    fun parsesALocationConstrainedRule() {
        val processorListJson =
            """
            {
              "processors": [
                {
                  "processor_type": "minecraft:rule",
                  "rules": [
                    {
                      "input_predicate": {
                        "block": "minecraft:dirt_path",
                        "predicate_type": "minecraft:block_match"
                      },
                      "location_predicate": {
                        "block": "minecraft:water",
                        "predicate_type": "minecraft:block_match"
                      },
                      "output_state": {
                        "Name": "minecraft:oak_planks"
                      }
                    }
                  ]
                }
              ]
            }
            """.trimIndent()

        val processors = VillagePoolJson.parseProcessorList(processorListJson)
        val dirtPathId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dirt_path"))!!.stateId
        val waterId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("water"))!!.stateId
        val planksId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("oak_planks"))!!.stateId
        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId

        assertEquals(planksId, processors.process(dirtPathId, waterId, fakeRandom()))
        assertEquals(
            dirtPathId,
            processors.process(dirtPathId, stoneId, fakeRandom()),
            "should not fire when the world block doesn't match location_predicate",
        )
    }
}
