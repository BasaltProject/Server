package org.basaltmc.world.loot

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.registry.Identifier

/** Thrown for loot table JSON this parser doesn't (yet) understand: an unsupported type, not malformed JSON. */
public class LootTableParseException(
    message: String,
) : Exception(message)

/**
 * Parses the real vanilla loot table JSON schema into [LootTable]. Only the entry/condition/
 * function types [LootEntry]/[LootCondition]/[LootFunction] themselves implement are recognized;
 * everything else throws [LootTableParseException] rather than silently producing wrong loot,
 * naming exactly what's missing so the gap is visible instead of a subtly incorrect drop table.
 */
public object LootTableJson {
    public fun parse(json: String): LootTable = parse(Json.parseToJsonElement(json) as JsonObject)

    public fun parse(root: JsonObject): LootTable =
        LootTable(
            pools = (root["pools"] as? JsonArray)?.map { parsePool(it as JsonObject) } ?: emptyList(),
            functions = (root["functions"] as? JsonArray)?.map { parseFunction(it as JsonObject) } ?: emptyList(),
        )

    private fun parsePool(pool: JsonObject): LootPool =
        LootPool(
            entries = (pool.getValue("entries") as JsonArray).map { parseEntry(it as JsonObject) },
            rolls = pool["rolls"]?.let(::parseNumberProvider) ?: NumberProvider.Constant(1f),
            bonusRolls = pool["bonus_rolls"]?.let(::parseNumberProvider) ?: NumberProvider.Constant(0f),
            conditions = (pool["conditions"] as? JsonArray)?.map { parseCondition(it as JsonObject) } ?: emptyList(),
            functions = (pool["functions"] as? JsonArray)?.map { parseFunction(it as JsonObject) } ?: emptyList(),
        )

    private fun parseEntry(entry: JsonObject): LootEntry {
        val conditions = (entry["conditions"] as? JsonArray)?.map { parseCondition(it as JsonObject) } ?: emptyList()
        return when (val type = entry.type()) {
            "minecraft:item" ->
                LootEntry(
                    item = Identifier.parse(entry.getValue("name").jsonPrimitive.content),
                    weight = (entry["weight"] as? JsonPrimitive)?.content?.toInt() ?: 1,
                    quality = (entry["quality"] as? JsonPrimitive)?.content?.toInt() ?: 0,
                    conditions = conditions,
                    functions =
                        (entry["functions"] as? JsonArray)?.map { parseFunction(it as JsonObject) } ?: emptyList(),
                )
            "minecraft:alternatives" ->
                LootEntry(
                    conditions = conditions,
                    alternatives = (entry.getValue("children") as JsonArray).map { parseEntry(it as JsonObject) },
                )
            else -> throw LootTableParseException("Unsupported loot pool entry type: $type")
        }
    }

    // Conditions dispatch on "condition", not "type" (`LootItemCondition.TYPED_CODEC`'s explicit
    // dispatch key, verified against the real jar's decompiled codec, unlike entries/number
    // providers which use the default "type" key).
    private fun parseCondition(condition: JsonObject): LootCondition {
        val type = condition.getValue("condition").jsonPrimitive.content
        return when (type) {
            "minecraft:random_chance" -> LootCondition.RandomChance(parseNumberProvider(condition.getValue("chance")))
            "minecraft:survives_explosion" -> LootCondition.SurvivesExplosion
            "minecraft:match_tool" -> LootCondition.MatchTool
            else -> throw LootTableParseException("Unsupported loot condition type: $type")
        }
    }

    // Functions dispatch on "function", not "type" (`LootItemFunctions.TYPED_CODEC`'s explicit
    // dispatch key, same deal as conditions above).
    private fun parseFunction(function: JsonObject): LootFunction {
        val type = function.getValue("function").jsonPrimitive.content
        return when (type) {
            "minecraft:set_count" ->
                LootFunction.SetCount(
                    count = parseNumberProvider(function.getValue("count")),
                    add = (function["add"] as? JsonPrimitive)?.content?.toBoolean() ?: false,
                )
            else -> throw LootTableParseException("Unsupported loot function type: $type")
        }
    }

    private fun parseNumberProvider(element: JsonElement): NumberProvider {
        if (element is JsonPrimitive) return NumberProvider.Constant(element.content.toFloat())
        val obj = element as JsonObject
        return when (val type = obj.type()) {
            "minecraft:constant" -> {
                val value = obj.getValue("value").jsonPrimitive.content
                NumberProvider.Constant(value.toFloat())
            }
            "minecraft:uniform" ->
                NumberProvider.Uniform(
                    min = parseNumberProvider(obj.getValue("min")),
                    max = parseNumberProvider(obj.getValue("max")),
                )
            else -> throw LootTableParseException("Unsupported number provider type: $type")
        }
    }

    private fun JsonObject.type(): String = getValue("type").jsonPrimitive.content
}
