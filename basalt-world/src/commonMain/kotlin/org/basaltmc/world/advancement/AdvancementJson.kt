package org.basaltmc.world.advancement

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.registry.Identifier

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
                name to
                    AdvancementCriterion(
                        Identifier.parse((value as JsonObject).getValue("trigger").jsonPrimitive.content),
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
