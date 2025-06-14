package org.basaltmc.server.world.advancement

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson

/**
 * Parses the real vanilla advancement JSON schema into [Advancement]. Missing `requirements`
 * defaults to one group per criterion (an implicit AND of everything), mirroring
 * `AdvancementRequirements.allOf`.
 */
public object AdvancementJson {
    public fun parse(json: String): Advancement = parse(Json.parseToJsonElement(json) as JsonObject)

    public fun parse(root: JsonObject): Advancement {
        val criteria =
            (root.getValue("criteria") as JsonObject).entries.associate { (name, value) ->
                val criterion = value as JsonObject
                val trigger = Identifier.parse(criterion.getValue("trigger").jsonPrimitive.content)
                name to
                    AdvancementCriterion(
                        trigger,
                        parseCondition(trigger, criterion["conditions"] as? JsonObject),
                    )
            }
        val requirements =
            (root["requirements"] as? JsonArray)?.let { groups ->
                AdvancementRequirements(groups.map { group -> (group as JsonArray).map { it.jsonPrimitive.content } })
            } ?: AdvancementRequirements(criteria.keys.map { listOf(it) })

        return Advancement(
            parent = (root["parent"] as? JsonPrimitive)?.content?.let(Identifier::parse),
            criteria = criteria,
            requirements = requirements,
            rewards = (root["rewards"] as? JsonObject)?.let(::parseRewards) ?: AdvancementRewards(),
            display = (root["display"] as? JsonObject)?.let(::parseDisplay),
        )
    }

    /**
     * The condition a trigger fires under, for the triggers this server can observe.
     *
     * parity: `CriterionTriggerInstance`, of which only `inventory_changed`'s item predicates and
     * `recipe_unlocked`'s recipe are read. Anything else becomes
     * [CriterionCondition.Unsupported] and never fires, rather than firing on the bare trigger and
     * handing out an advancement nobody earned.
     */
    private fun parseCondition(
        trigger: Identifier,
        conditions: JsonObject?,
    ): CriterionCondition =
        when (trigger.toString()) {
            "minecraft:inventory_changed" -> {
                val items = conditions?.get("items") as? JsonArray
                if (items == null) {
                    CriterionCondition.Unsupported(trigger)
                } else {
                    CriterionCondition.InventoryItems(items.mapNotNull { parseMatcher(it as? JsonObject) })
                }
            }
            "minecraft:recipe_unlocked" -> {
                val recipe = (conditions?.get("recipe") as? JsonPrimitive)?.content
                recipe?.let { CriterionCondition.RecipeUnlocked(Identifier.parse(it)) }
                    ?: CriterionCondition.Unsupported(trigger)
            }
            else -> CriterionCondition.Unsupported(trigger)
        }

    /** `items` is one id, a `#tag`, or a list of ids. parity: `ItemPredicate`'s own `HolderSet` field. */
    private fun parseMatcher(predicate: JsonObject?): ItemMatcher? {
        val items = predicate?.get("items") ?: return null
        if (items is JsonArray) {
            return ItemMatcher(items = items.map { Identifier.parse(it.jsonPrimitive.content) })
        }
        val value = (items as? JsonPrimitive)?.content ?: return null
        return if (value.startsWith("#")) {
            ItemMatcher(tag = Identifier.parse(value.removePrefix("#")))
        } else {
            ItemMatcher(items = listOf(Identifier.parse(value)))
        }
    }

    private fun parseDisplay(display: JsonObject): AdvancementDisplay? {
        val title = display["title"]?.let(ComponentJson::deserialize) ?: return null
        return AdvancementDisplay(
            title = title,
            description = display["description"]?.let(ComponentJson::deserialize) ?: Component.empty(),
            type = (display["frame"] as? JsonPrimitive)?.content ?: "task",
            announceToChat = (display["announce_to_chat"] as? JsonPrimitive)?.content?.toBoolean() ?: true,
        )
    }

    private fun parseRewards(rewards: JsonObject): AdvancementRewards =
        AdvancementRewards(
            experience = (rewards["experience"] as? JsonPrimitive)?.content?.toInt() ?: 0,
            loot = (rewards["loot"] as? JsonArray)?.map { Identifier.parse(it.jsonPrimitive.content) } ?: emptyList(),
            recipes =
                (rewards["recipes"] as? JsonArray)?.map { Identifier.parse(it.jsonPrimitive.content) } ?: emptyList(),
        )
}
