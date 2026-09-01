package org.basaltmc.server.world.loot

import org.basaltmc.server.random.LegacyRandom
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the loot evaluator and its JSON schema. */
class LootOracle {
    /**
     * Tables written the way the real ones are: weighted alternatives, a rolled count, a bonus roll
     * that only luck reaches, and the conditions and functions the evaluator answers by fiat.
     */
    private val tables =
        mapOf(
            "simple" to
                """{"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:diamond"}]}]}""",
            "counted" to
                """{"pools":[{"rolls":{"type":"minecraft:uniform","min":2,"max":5},"entries":[
                  {"type":"minecraft:item","name":"minecraft:coal","functions":[
                    {"function":"minecraft:set_count","count":{"type":"minecraft:uniform","min":1,"max":4}}]}]}]}""",
            "weighted" to
                """{"pools":[{"rolls":3,"entries":[
                  {"type":"minecraft:item","name":"minecraft:iron_ingot","weight":10},
                  {"type":"minecraft:item","name":"minecraft:gold_ingot","weight":5,"quality":2},
                  {"type":"minecraft:item","name":"minecraft:emerald","weight":1}]}]}""",
            "alternatives" to
                """{"pools":[{"rolls":2,"entries":[
                  {"type":"minecraft:alternatives","children":[
                    {"type":"minecraft:item","name":"minecraft:stone","conditions":[
                      {"condition":"minecraft:match_tool"}]},
                    {"type":"minecraft:item","name":"minecraft:cobblestone"}]},
                  {"type":"minecraft:item","name":"minecraft:gravel","weight":3}]}]}""",
            "chances" to
                """{"pools":[{"rolls":4,"entries":[
                  {"type":"minecraft:item","name":"minecraft:bone","conditions":[
                    {"condition":"minecraft:random_chance","chance":0.5}]},
                  {"type":"minecraft:item","name":"minecraft:string","conditions":[
                    {"condition":"minecraft:inverted","term":{"condition":"minecraft:killed_by_player"}}]},
                  {"type":"minecraft:item","name":"minecraft:rotten_flesh","conditions":[
                    {"condition":"minecraft:any_of","terms":[
                      {"condition":"minecraft:entity_properties"},
                      {"condition":"minecraft:survives_explosion"}]}]},
                  {"type":"minecraft:item","name":"minecraft:feather","conditions":[
                    {"condition":"minecraft:all_of","terms":[
                      {"condition":"minecraft:killed_by_player"},
                      {"condition":"minecraft:random_chance_with_enchanted_bonus",
                       "unenchanted_chance":0.25}]}]}]}]}""",
            "explosive" to
                """{"pools":[{"rolls":2,"conditions":[{"condition":"minecraft:survives_explosion"}],"entries":[
                  {"type":"minecraft:item","name":"minecraft:sand","functions":[
                    {"function":"minecraft:set_count","count":8}]}]}],
                 "functions":[{"function":"minecraft:explosion_decay"}]}""",
            "gated" to
                """{"pools":[{"rolls":1,"conditions":[{"condition":"minecraft:match_tool"}],"entries":[
                  {"type":"minecraft:item","name":"minecraft:glass"}]}]}""",
            // Bounds that meet, a fractional count, a negative quality and an alternatives whose
            // first two children both pass: the four shapes the plain tables never reach.
            "edges" to
                """{"pools":[{"rolls":{"type":"minecraft:uniform","min":3,"max":3},"entries":[
                  {"type":"minecraft:item","name":"minecraft:clay_ball","weight":8,"quality":-3,"functions":[
                    {"function":"minecraft:set_count","count":2.7}]},
                  {"type":"minecraft:alternatives","weight":20,"children":[
                    {"type":"minecraft:item","name":"minecraft:brick"},
                    {"type":"minecraft:item","name":"minecraft:flint"},
                    {"type":"minecraft:item","name":"minecraft:quartz"}]},
                  {"type":"minecraft:item","name":"minecraft:clay","weight":4,"conditions":[
                    {"condition":"minecraft:random_chance",
                     "chance":{"type":"minecraft:uniform","min":0.5,"max":0.5}}],"functions":[
                    {"function":"minecraft:set_count",
                     "count":{"type":"minecraft:uniform","min":2.5,"max":2.5}}]}]}]}""",
            "bonus" to
                """{"pools":[{"rolls":1,"bonus_rolls":2,"entries":[
                  {"type":"minecraft:item","name":"minecraft:lapis_lazuli","functions":[
                    {"function":"minecraft:enchanted_count_increase","count":1,"limit":9},
                    {"function":"minecraft:copy_name"},
                    {"function":"minecraft:furnace_smelt"}]}]}]}""",
        )

    private fun report(
        name: String,
        seed: Long,
        luck: Float,
        explosionRadius: Float?,
        table: String = name,
    ) {
        val parsed = LootTableJson.parse(tables.getValue(table))
        val random = LegacyRandom(seed)
        val drops = parsed.generate(random, luck, explosionRadius)
        var checksum = -3750763034362895579L
        for (drop in drops) {
            for (char in drop.item.toString()) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            checksum = (checksum xor drop.count.toLong()) * 1099511628211L
        }
        val total = drops.sumOf { it.count }
        println("ORACLE $name drops=${drops.size} total=$total after=${random.nextInt()} checksum=$checksum")
    }

    private fun reportRefused(
        name: String,
        json: String,
    ) {
        val message =
            try {
                LootTableJson.parse(json)
                "accepted"
            } catch (failure: LootTableParseException) {
                failure.message ?: "unknown"
            }
        println("ORACLE $name refused=${message.replace(' ', '_')}")
    }

    @Test
    fun dump() {
        report("simple", 1234L, 0f, null)
        report("counted", 1234L, 0f, null)
        report("weighted", 1234L, 0f, null)
        report("weightedLucky", 1234L, 3f, null, table = "weighted")
        report("alternatives", 1234L, 0f, null)
        report("chances", 1234L, 0f, null)
        report("explosive", 1234L, 0f, null)
        report("explosiveBlast", 1234L, 0f, 4f, table = "explosive")
        report("gated", 1234L, 0f, null)
        report("bonus", 1234L, 0f, null)
        report("bonusLucky", 1234L, 4f, null, table = "bonus")
        report("countedOther", 77L, 0f, null, table = "counted")
        report("edges", 1234L, 0f, null)
        // Luck three times a negative quality drives that entry's weight below zero.
        report("edgesLucky", 1234L, 3f, null, table = "edges")
        // A wide enough blast swallows a whole stack, which is then not a drop at all.
        report("explosiveWide", 1234L, 0f, 12f, table = "explosive")
        reportRefused(
            "unknownEntry",
            """{"pools":[{"rolls":1,"entries":[{"type":"minecraft:tag","name":"minecraft:logs"}]}]}""",
        )
        reportRefused(
            "unknownCondition",
            """{"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
               "conditions":[{"condition":"minecraft:table_bonus"}]}]}]}""",
        )
        reportRefused(
            "unknownFunction",
            """{"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
               "functions":[{"function":"minecraft:enchant_randomly"}]}]}]}""",
        )
        reportRefused(
            "unknownProvider",
            """{"pools":[{"rolls":{"type":"minecraft:binomial","n":3,"p":0.5},"entries":[
               {"type":"minecraft:item","name":"minecraft:stone"}]}]}""",
        )
    }
}
