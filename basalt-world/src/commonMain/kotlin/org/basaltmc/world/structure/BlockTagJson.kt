package org.basaltmc.world.structure

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

/** Parses a real `data/minecraft/tags/block/<name>.json`: just its `values` list, entries either `minecraft:block` or `#minecraft:other_tag`. */
public object BlockTagJson {
    public fun parse(json: String): List<String> =
        Json.parseToJsonElement(json).jsonObject["values"]?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
}
