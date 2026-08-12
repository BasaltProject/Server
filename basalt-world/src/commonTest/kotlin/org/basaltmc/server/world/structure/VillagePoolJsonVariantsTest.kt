package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class VillagePoolJsonVariantsTest {
    private fun fakeRandom() =
        org.basaltmc.server.random
            .WorldgenRandom(
                org.basaltmc.server.random
                    .XoroshiroRandom(1L),
            )

    @Test
    fun featurePoolElementParses() {
        val pool =
            VillagePoolJson.parsePool(
                """
                {
                  "fallback": "minecraft:empty",
                  "elements": [
                    {
                      "weight": 3,
                      "element": {
                        "element_type": "minecraft:feature_pool_element",
                        "feature": "minecraft:pile_hay",
                        "projection": "terrain_matching"
                      }
                    }
                  ]
                }
                """.trimIndent(),
            ) { null }
        assertEquals(1, pool.size() / 3)
        val element = pool.getShuffledTemplates(fakeRandom()).first()
        val featureElement = assertIs<FeaturePoolElement>(element)
        assertEquals("minecraft:pile_hay", featureElement.feature)
        assertEquals(StructureProjection.TERRAIN_MATCHING, featureElement.projection)
        val connector =
            featureElement
                .getShuffledJigsawBlocks({ null }, StructurePos(7, 8, 9), StructureRotation.CLOCKWISE_90, fakeRandom())
                .single()
        assertEquals(StructurePos(7, 8, 9), connector.pos)
        assertEquals(StructureDirection.DOWN, connector.front)
        assertEquals(StructureDirection.SOUTH, connector.top)
        assertEquals("minecraft:bottom", connector.name)
        assertTrue(connector.rollable)
    }

    @Test
    fun tagMatchAndBlockstateMatchParse() {
        val doors = setOf(101, 102)
        val list =
            VillagePoolJson.parseProcessorList(
                """
                {
                  "processors": [
                    {
                      "processor_type": "minecraft:rule",
                      "rules": [
                        {
                          "input_predicate": { "predicate_type": "minecraft:tag_match", "tag": "minecraft:doors" },
                          "location_predicate": { "predicate_type": "minecraft:always_true" },
                          "output_state": { "Name": "minecraft:air" }
                        },
                        {
                          "input_predicate": { "predicate_type": "minecraft:blockstate_match", "block_state": { "Name": "minecraft:cobblestone", "Properties": {} } },
                          "location_predicate": { "predicate_type": "minecraft:always_true" },
                          "output_state": { "Name": "minecraft:mossy_cobblestone" }
                        }
                      ]
                    }
                  ]
                }
                """.trimIndent(),
            ) { tag -> if (tag == "minecraft:doors") doors else null }
        assertEquals(2, list.rules.size)
        val tagRule = assertIs<StructureRuleTest.TagMatch>(list.rules[0].inputTest)
        assertEquals(doors, tagRule.stateIds)
        assertTrue(list.rules[1].inputTest is StructureRuleTest.BlockStateMatch)
    }

    @Test
    fun zombiePaneRuleMatchesAndOutputsExactPropertyStates() {
        val list =
            VillagePoolJson.parseProcessorList(
                """
                {
                  "processors": [{
                    "processor_type": "minecraft:rule",
                    "rules": [{
                      "input_predicate": {
                        "predicate_type": "minecraft:blockstate_match",
                        "block_state": {
                          "Name": "minecraft:glass_pane",
                          "Properties": {"east":"false","north":"true","south":"true","waterlogged":"false","west":"false"}
                        }
                      },
                      "location_predicate": {"predicate_type":"minecraft:always_true"},
                      "output_state": {
                        "Name": "minecraft:brown_stained_glass_pane",
                        "Properties": {"east":"false","north":"true","south":"true","waterlogged":"false","west":"false"}
                      }
                    }]
                  }]
                }
                """.trimIndent(),
            )

        assertEquals(11863, list.process(8319, Blocks.AIR.stateId, fakeRandom()))
        assertEquals(8314, list.process(8314, Blocks.AIR.stateId, fakeRandom()))
    }
}
