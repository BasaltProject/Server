package org.basaltmc.server.world.advancement

import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the advancement schema and its completion test. */
class AdvancementOracle {
    private val files =
        mapOf(
            // No requirements at all, so they are built one group per criterion, in file order.
            "root" to
                """{"display":{"title":{"translate":"advancements.story.root.title"},
                    "description":"Sleep tonight","frame":"task","announce_to_chat":"false"},
                   "criteria":{"crafting_table":{"trigger":"minecraft:inventory_changed",
                     "conditions":{"items":[{"items":"minecraft:crafting_table"}]}},
                    "planks":{"trigger":"minecraft:inventory_changed",
                     "conditions":{"items":[{"items":"#minecraft:planks"},
                                            {"items":["minecraft:stick","minecraft:flint"]}]}}}}""",
            // An explicit OR: either of the two grants it.
            "either" to
                """{"parent":"minecraft:story/root",
                   "criteria":{"stone":{"trigger":"minecraft:recipe_unlocked",
                     "conditions":{"recipe":"minecraft:stone_bricks"}},
                    "brick":{"trigger":"minecraft:recipe_unlocked",
                     "conditions":{"recipe":"minecraft:bricks"}}},
                   "requirements":[["stone","brick"]],
                   "rewards":{"experience":40,"loot":["minecraft:chests/end_city_treasure"],
                              "recipes":["minecraft:elytra"]}}""",
            // An AND of two ORs, which is what the big collection advancements look like.
            "grouped" to
                """{"criteria":{"a":{"trigger":"minecraft:tick"},"b":{"trigger":"minecraft:tick"},
                    "c":{"trigger":"minecraft:tick"},"d":{"trigger":"minecraft:tick"}},
                   "requirements":[["a","b"],["c","d"]]}""",
            // An empty requirements array, which is not the same as leaving it out: no group means
            // nothing to satisfy, and vanilla answers that with never complete rather than always.
            "nothing" to
                """{"criteria":{"a":{"trigger":"minecraft:tick"}},"requirements":[]}""",
            // Requirements naming nothing: no group can be satisfied, so it is never complete.
            "impossible" to
                """{"criteria":{"a":{"trigger":"minecraft:impossible"}},"requirements":[[]]}""",
            // A trigger with no event behind it, and two that carry no conditions block at all.
            "unsupported" to
                """{"criteria":{"bred":{"trigger":"minecraft:bred_animals",
                     "conditions":{"parent":{"type":"minecraft:cow"}}},
                    "bare":{"trigger":"minecraft:inventory_changed"},
                    "bareRecipe":{"trigger":"minecraft:recipe_unlocked","conditions":{}}},
                   "requirements":[["bred"],["bare"],["bareRecipe"]]}""",
            // A display whose frame and announcement are left out, so both take their defaults.
            "plain" to
                """{"display":{"title":"Adventuring Time","description":{"text":"Discover every biome"}},
                   "criteria":{"a":{"trigger":"minecraft:tick"}},"rewards":{"experience":500}}""",
            // A display with no title, which vanilla treats as no display rather than a broken one.
            "titleless" to
                """{"display":{"description":"Nothing to show"},
                   "criteria":{"a":{"trigger":"minecraft:tick"}}}""",
        )

    private fun checksum(text: String): Long {
        var checksum = -3750763034362895579L
        for (char in text) checksum = (checksum xor char.code.toLong()) * 1099511628211L
        return checksum
    }

    private fun describe(condition: CriterionCondition): String =
        when (condition) {
            is CriterionCondition.InventoryItems ->
                "items[" +
                    condition.matchers.joinToString(";") { matcher ->
                        matcher.items.joinToString(",") + "|" + (matcher.tag?.toString() ?: "-")
                    } + "]"
            is CriterionCondition.RecipeUnlocked -> "recipe[${condition.recipe}]"
            is CriterionCondition.Unsupported -> "unsupported[${condition.trigger}]"
        }

    private fun report(
        name: String,
        granted: List<Set<String>>,
    ) {
        val advancement = AdvancementJson.parse(files.getValue(name))
        val criteria =
            advancement.criteria.entries.joinToString(";") { (key, criterion) ->
                "$key=${criterion.trigger}/${describe(criterion.condition)}"
            }
        val groups = advancement.requirements.groups.joinToString(";") { it.joinToString(",") }
        val rewards =
            "${advancement.rewards.experience}/" +
                advancement.rewards.loot.joinToString(",") + "/" + advancement.rewards.recipes.joinToString(",")
        val display =
            advancement.display?.let {
                PlainTextSerializer.serialize(it.title) + "/" + PlainTextSerializer.serialize(it.description) +
                    "/" + it.type + "/" + it.announceToChat
            } ?: "-"
        val complete = granted.joinToString(",") { advancement.isComplete(it).toString() }
        println(
            "ORACLE $name parent=${advancement.parent} criteria=$criteria groups=$groups " +
                "rewards=$rewards display=$display complete=$complete " +
                "checksum=${checksum("$criteria$groups$rewards$display$complete")}",
        )
    }

    private fun reportRefused(
        name: String,
        json: String,
    ) {
        val message =
            try {
                AdvancementJson.parse(json)
                "accepted"
            } catch (failure: Exception) {
                failure::class.simpleName ?: "unknown"
            }
        println("ORACLE $name refused=$message")
    }

    @Test
    fun dump() {
        report("root", listOf(emptySet(), setOf("planks"), setOf("crafting_table", "planks")))
        report("either", listOf(emptySet(), setOf("stone"), setOf("stone", "brick")))
        report(
            "grouped",
            listOf(emptySet(), setOf("a"), setOf("a", "c"), setOf("b", "d"), setOf("a", "b"), setOf("a", "b", "c")),
        )
        report("nothing", listOf(emptySet(), setOf("a")))
        report("impossible", listOf(emptySet(), setOf("a")))
        report("unsupported", listOf(emptySet(), setOf("bred", "bare", "bareRecipe")))
        report("plain", listOf(emptySet(), setOf("a")))
        report("titleless", listOf(setOf("a")))
        reportRefused("noCriteria", """{"display":{"title":"x"}}""")
        reportRefused("noTrigger", """{"criteria":{"a":{}}}""")
    }
}
