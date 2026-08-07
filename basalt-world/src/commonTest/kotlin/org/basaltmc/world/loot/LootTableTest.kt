package org.basaltmc.world.loot

import org.basaltmc.random.LegacyRandom
import org.basaltmc.registry.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class LootTableTest {
    @Test
    fun aSingleEntryPoolAlwaysProducesThatItem() {
        val table =
            LootTable(
                pools = listOf(LootPool(entries = listOf(LootEntry(item = Identifier.vanilla("diamond"))))),
            )
        val result = table.generate(LegacyRandom(1L))

        assertEquals(1, result.size)
        assertEquals(Identifier.vanilla("diamond"), result[0].item)
        assertEquals(1, result[0].count)
    }

    @Test
    fun aSingleEntryPoolNeverRollsTheWeightedRandomCall() {
        // Real vanilla's LootPool.addRandomItem skips `random.nextInt(totalWeight)` entirely when
        // there's exactly one valid entry (see LootPool.java's `entryCount == 1` shortcut): confirm
        // that by checking a subsequent, unrelated random draw matches an untouched generator.
        val seed = 999L
        val table = LootTable(pools = listOf(LootPool(entries = listOf(LootEntry(item = Identifier.vanilla("stone"))))))
        val random = LegacyRandom(seed)
        table.generate(random)
        val afterGenerate = random.nextInt(1000)

        val untouched = LegacyRandom(seed)
        val expected = untouched.nextInt(1000)

        assertEquals(expected, afterGenerate)
    }

    @Test
    fun weightedSelectionMatchesNextIntTotalWeightThenLinearScan() {
        // Mirrors WeightedRandom.getRandomItem's exact algorithm: random.nextInt(totalWeight),
        // then subtract each entry's weight in order until the running index goes negative.
        val entries =
            listOf(
                LootEntry(item = Identifier.vanilla("a"), weight = 1),
                LootEntry(item = Identifier.vanilla("b"), weight = 2),
                LootEntry(item = Identifier.vanilla("c"), weight = 3),
            )
        val seed = 42L
        val table = LootTable(pools = listOf(LootPool(entries = entries)))

        val prediction = LegacyRandom(seed)
        var index = prediction.nextInt(6) // totalWeight = 1 + 2 + 3
        val expected =
            entries.first { entry ->
                index -= entry.weight
                index < 0
            }

        val result = table.generate(LegacyRandom(seed))
        assertEquals(expected.item, result[0].item)
    }

    @Test
    fun setCountUniformMatchesNextIntBetweenInclusive() {
        val seed = 7L
        val entry =
            LootEntry(
                item = Identifier.vanilla("arrow"),
                functions =
                    listOf(
                        LootFunction.SetCount(
                            NumberProvider.Uniform(NumberProvider.Constant(1f), NumberProvider.Constant(3f)),
                        ),
                    ),
            )
        val table = LootTable(pools = listOf(LootPool(entries = listOf(entry))))

        val prediction = LegacyRandom(seed)
        val expectedCount = prediction.nextIntBetweenInclusive(1, 3)

        val result = table.generate(LegacyRandom(seed))
        assertEquals(expectedCount, result[0].count)
    }

    @Test
    fun randomChanceConditionMatchesNextFloatComparison() {
        val seed = 123L
        val entry =
            LootEntry(
                item = Identifier.vanilla("emerald"),
                conditions = listOf(LootCondition.RandomChance(NumberProvider.Constant(0.5f))),
            )
        val table = LootTable(pools = listOf(LootPool(entries = listOf(entry))))

        val prediction = LegacyRandom(seed)
        val shouldPass = prediction.nextFloat() < 0.5f

        val result = table.generate(LegacyRandom(seed))
        assertEquals(if (shouldPass) 1 else 0, result.size)
    }

    @Test
    fun multipleRollsProduceThatManyItems() {
        val table =
            LootTable(
                pools =
                    listOf(
                        LootPool(
                            entries = listOf(LootEntry(item = Identifier.vanilla("gravel"))),
                            rolls = NumberProvider.Constant(4f),
                        ),
                    ),
            )
        val result = table.generate(LegacyRandom(5L))
        assertEquals(4, result.size)
        assertTrue(result.all { it.item == Identifier.vanilla("gravel") })
    }

    @Test
    fun bonusRollsScaleWithLuck() {
        val table =
            LootTable(
                pools =
                    listOf(
                        LootPool(
                            entries = listOf(LootEntry(item = Identifier.vanilla("cod"))),
                            rolls = NumberProvider.Constant(1f),
                            bonusRolls = NumberProvider.Constant(2f),
                        ),
                    ),
            )
        val result = table.generate(LegacyRandom(3L), luck = 1.5f)
        // 1 + floor(2 * 1.5) = 1 + 3 = 4
        assertEquals(4, result.size)
    }

    @Test
    fun parsesARealShapedLootTableJsonAndEvaluatesIt() {
        val json =
            """
            {
                "pools": [
                    {
                        "rolls": 1,
                        "entries": [
                            {
                                "type": "minecraft:item",
                                "name": "minecraft:flint",
                                "functions": [
                                    { "function": "minecraft:set_count", "count": { "type": "minecraft:uniform", "min": 1, "max": 2 } }
                                ]
                            }
                        ]
                    }
                ]
            }
            """.trimIndent()
        val table = LootTableJson.parse(json)
        val result = table.generate(LegacyRandom(2026L))

        assertEquals(1, result.size)
        assertEquals(Identifier.vanilla("flint"), result[0].item)
        assertTrue(result[0].count in 1..2)
    }

    @Test
    fun unsupportedEntryTypesThrowWithTheTypeName() {
        val json = """{"pools":[{"entries":[{"type":"minecraft:tag","name":"minecraft:logs"}]}]}"""
        try {
            LootTableJson.parse(json)
            error("expected LootTableParseException")
        } catch (e: LootTableParseException) {
            assertTrue(e.message!!.contains("minecraft:tag"))
        }
    }
}
