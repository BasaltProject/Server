package org.basaltmc.server.world.loot

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.wire.common.Identifier
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
    fun matchesTheRealVanilla262ServerForAFixedSeed() {
        // Live parity check, not just formula-by-formula against decompiled source: this exact
        // pool (rolls=5, stone/dirt/sand at weight 1/2/3) was loaded into a real vanilla 26.2
        // server as a datapack loot table, put on a chest with a fixed LootTableSeed of 2026 (the
        // real jar's `RandomizableContainer`/`LootTable.fill` mechanism, which uses
        // `RandomSource.create(seed)`, i.e. `LegacyRandomSource`, confirmed by decompiling it
        // rather than assumed), and unpacked via RCON. The real server produced, in roll order:
        // sand, sand, dirt, stone, sand. This engine produces the identical sequence for the
        // identical seed.
        val entries =
            listOf(
                LootEntry(item = Identifier.vanilla("stone"), weight = 1),
                LootEntry(item = Identifier.vanilla("dirt"), weight = 2),
                LootEntry(item = Identifier.vanilla("sand"), weight = 3),
            )
        val table =
            LootTable(
                pools = listOf(LootPool(entries = entries, rolls = NumberProvider.Constant(5f))),
            )

        val result = table.generate(LegacyRandom(2026L))

        val expected =
            listOf("sand", "sand", "dirt", "stone", "sand").map { Identifier.vanilla(it) }
        assertEquals(expected, result.map { it.item })
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

    @Test
    fun survivesExplosionAlwaysPasses() {
        val entry =
            LootEntry(item = Identifier.vanilla("dirt"), conditions = listOf(LootCondition.SurvivesExplosion))
        val table = LootTable(pools = listOf(LootPool(entries = listOf(entry))))

        val result = table.generate(LegacyRandom(1L))

        assertEquals(1, result.size)
    }

    @Test
    fun matchToolNeverPassesSinceEnchantmentsAreNotWiredYet() {
        val entry = LootEntry(item = Identifier.vanilla("stone"), conditions = listOf(LootCondition.MatchTool))
        val table = LootTable(pools = listOf(LootPool(entries = listOf(entry))))

        val result = table.generate(LegacyRandom(1L))

        assertEquals(0, result.size)
    }

    @Test
    fun alternativesEntryResolvesToTheFirstChildWhoseConditionsPass() {
        // Mirrors a real "stone" style loot table: silk touch (never true here, see
        // [matchToolNeverPassesSinceEnchantmentsAreNotWiredYet]) falls through to the plain drop.
        val entry =
            LootEntry(
                alternatives =
                    listOf(
                        LootEntry(item = Identifier.vanilla("stone"), conditions = listOf(LootCondition.MatchTool)),
                        LootEntry(
                            item = Identifier.vanilla("cobblestone"),
                            conditions = listOf(LootCondition.SurvivesExplosion),
                        ),
                    ),
            )
        val table = LootTable(pools = listOf(LootPool(entries = listOf(entry))))

        val result = table.generate(LegacyRandom(1L))

        assertEquals(1, result.size)
        assertEquals(Identifier.vanilla("cobblestone"), result[0].item)
    }

    @Test
    fun parsesAnAlternativesEntryFromJson() {
        val json =
            """
            {
                "pools": [
                    {
                        "entries": [
                            {
                                "type": "minecraft:alternatives",
                                "children": [
                                    {
                                        "type": "minecraft:item",
                                        "conditions": [{ "condition": "minecraft:match_tool" }],
                                        "name": "minecraft:grass_block"
                                    },
                                    {
                                        "type": "minecraft:item",
                                        "conditions": [{ "condition": "minecraft:survives_explosion" }],
                                        "name": "minecraft:dirt"
                                    }
                                ]
                            }
                        ]
                    }
                ]
            }
            """.trimIndent()
        val table = LootTableJson.parse(json)

        val result = table.generate(LegacyRandom(1L))

        assertEquals(1, result.size)
        assertEquals(Identifier.vanilla("dirt"), result[0].item)
    }
}
