package org.basaltmc.world.crafting

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.common.Identifier
import org.basaltmc.world.loot.ItemStack

/** Thrown for recipe JSON this parser doesn't (yet) understand: an unsupported type, not malformed JSON. */
public class RecipeParseException(
    message: String,
) : Exception(message)

/**
 * Parses the real vanilla crafting recipe JSON schema into [Recipe]. Only `minecraft:crafting_shaped`
 * and `minecraft:crafting_shapeless` are recognized; furnace/stonecutter/smithing/special recipes
 * throw [RecipeParseException] naming the unsupported type instead of silently producing nothing.
 *
 * Ingredients follow vanilla's `HolderSetCodec`: a bare `"minecraft:stick"` (one item), `"#minecraft:
 * planks"` (a tag reference, resolved through [resolveTag]), or a JSON array of either.
 */
public object RecipeJson {
    public fun parse(
        json: String,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Recipe = parse(Json.parseToJsonElement(json) as JsonObject, resolveTag)

    public fun parse(
        root: JsonObject,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Recipe {
        val type = root.getValue("type").jsonPrimitive.content
        val group = (root["group"] as? JsonPrimitive)?.content ?: ""
        val result = parseResult(root.getValue("result") as JsonObject)
        return when (type) {
            "minecraft:crafting_shaped" -> parseShaped(root, group, result, resolveTag)
            "minecraft:crafting_shapeless" -> parseShapeless(root, group, result, resolveTag)
            else -> throw RecipeParseException("Unsupported recipe type: $type")
        }
    }

    private fun parseResult(result: JsonObject): ItemStack =
        ItemStack(
            item = Identifier.parse(result.getValue("id").jsonPrimitive.content),
            count = (result["count"] as? JsonPrimitive)?.content?.toInt() ?: 1,
        )

    private fun parseShaped(
        root: JsonObject,
        group: String,
        result: ItemStack,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Recipe.Shaped {
        val key =
            (root.getValue("key") as JsonObject).entries.associate { (symbol, value) ->
                require(symbol.length == 1) { "Invalid pattern key symbol '$symbol': must be 1 character" }
                symbol[0] to parseIngredient(value, resolveTag)
            }
        val pattern = shrinkPattern((root.getValue("pattern") as JsonArray).map { it.jsonPrimitive.content })
        val width = pattern[0].length
        val height = pattern.size

        val ingredients = mutableListOf<Ingredient?>()
        for (line in pattern) {
            for (symbol in line) {
                ingredients +=
                    if (symbol == ' ') {
                        null
                    } else {
                        key[symbol]
                            ?: throw RecipeParseException(
                                "Pattern references symbol '$symbol' but it's not defined in the key",
                            )
                    }
            }
        }
        return Recipe.Shaped(group, result, width, height, ingredients)
    }

    private fun parseShapeless(
        root: JsonObject,
        group: String,
        result: ItemStack,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Recipe.Shapeless {
        val ingredients = (root.getValue("ingredients") as JsonArray).map { parseIngredient(it, resolveTag) }
        return Recipe.Shapeless(group, result, ingredients)
    }

    private fun parseIngredient(
        element: JsonElement,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Ingredient =
        when (element) {
            is JsonPrimitive -> parseIngredientToken(element.content, resolveTag)
            is JsonArray ->
                Ingredient(
                    element.flatMap { parseIngredientToken(it.jsonPrimitive.content, resolveTag).items }.toSet(),
                )
            else -> throw RecipeParseException("Unsupported ingredient JSON: $element")
        }

    private fun parseIngredientToken(
        raw: String,
        resolveTag: (Identifier) -> Set<Identifier>,
    ): Ingredient =
        if (raw.startsWith("#")) {
            Ingredient(resolveTag(Identifier.parse(raw.substring(1))))
        } else {
            Ingredient(setOf(Identifier.parse(raw)))
        }

    // Mirrors `ShapedRecipePattern.shrink`: trims blank leading/trailing rows and columns from the
    // authored pattern before it's turned into a fixed width/height ingredient grid.
    private fun shrinkPattern(pattern: List<String>): List<String> {
        var left = Int.MAX_VALUE
        var right = 0
        var top = 0
        var bottom = 0
        for (i in pattern.indices) {
            val line = pattern[i]
            left = minOf(left, firstNonEmpty(line))
            val lastNonSpace = lastNonEmpty(line)
            right = maxOf(right, lastNonSpace)
            if (lastNonSpace < 0) {
                if (top == i) top++
                bottom++
            } else {
                bottom = 0
            }
        }
        if (pattern.size == bottom) return emptyList()
        return (0 until pattern.size - bottom - top).map { line -> pattern[line + top].substring(left, right + 1) }
    }

    private fun firstNonEmpty(line: String): Int {
        var index = 0
        while (index < line.length && line[index] == ' ') index++
        return index
    }

    private fun lastNonEmpty(line: String): Int {
        var index = line.length - 1
        while (index >= 0 && line[index] == ' ') index--
        return index
    }
}
