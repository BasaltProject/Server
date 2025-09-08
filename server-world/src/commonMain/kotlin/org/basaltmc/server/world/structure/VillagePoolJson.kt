package org.basaltmc.server.world.structure

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** Thrown for a `worldgen/template_pool`/`processor_list` JSON shape this parser doesn't understand. */
public class VillagePoolParseException(
    message: String,
) : Exception(message)

/**
 * Parses the real 26.2 `worldgen/template_pool` and `worldgen/processor_list` JSON formats used by
 * villages and Bastion. Supported elements are single, legacy single, list, feature and empty;
 * processor lists retain rule processors and every rule predicate used by the 26.2 village and
 * Bastion processor corpus.
 */
public object VillagePoolJson {
    /** [resolveProcessors] resolves a named processor list reference (`"minecraft:mossify_20_percent"`) to its real JSON text; only called for string-form `processors` fields. [resolveTag] resolves a block tag name (`"minecraft:doors"`) to its member block state ids; only called for `tag_match` rule predicates. */
    public fun parsePool(
        json: String,
        resolveTag: (String) -> Set<Int>? = { null },
        resolveProcessors: (String) -> String?,
    ): StructureTemplatePool {
        val root = Json.parseToJsonElement(json).jsonObject
        val fallback = root["fallback"]?.jsonPrimitive?.content ?: "minecraft:empty"
        val elements = root["elements"]?.jsonArray ?: JsonArray(emptyList())
        val weighted =
            elements.map { entry ->
                val entryObj = entry.jsonObject
                val weight = entryObj["weight"]?.jsonPrimitive?.content?.toIntOrNull() ?: 1
                val elementObj = entryObj["element"]?.jsonObject ?: throw VillagePoolParseException("Missing 'element'")
                parseElement(elementObj, resolveProcessors, resolveTag) to weight
            }
        return StructureTemplatePool(fallback, weighted)
    }

    private fun parseElement(
        json: JsonObject,
        resolveProcessors: (String) -> String?,
        resolveTag: (String) -> Set<Int>?,
    ): StructurePoolElement {
        val type =
            json["element_type"]?.jsonPrimitive?.content ?: throw VillagePoolParseException("Missing 'element_type'")
        return when (type) {
            "minecraft:empty_pool_element" -> EmptyPoolElement
            "minecraft:feature_pool_element" -> {
                val feature =
                    json["feature"]?.jsonPrimitive?.content ?: throw VillagePoolParseException("Missing 'feature'")
                FeaturePoolElement(feature, parseProjection(json))
            }
            "minecraft:legacy_single_pool_element", "minecraft:single_pool_element" -> {
                val location =
                    json["location"]?.jsonPrimitive?.content ?: throw VillagePoolParseException("Missing 'location'")
                val projection = parseProjection(json)
                val processors = parseProcessorsField(json["processors"], resolveProcessors, resolveTag)
                LegacySinglePoolElement(
                    location,
                    processors,
                    projection,
                    ignoreAir = type == "minecraft:legacy_single_pool_element",
                )
            }
            "minecraft:list_pool_element" -> {
                val elements = json["elements"]?.jsonArray ?: JsonArray(emptyList())
                val projection = parseProjection(json)
                ListPoolElement(elements.map { parseElement(it.jsonObject, resolveProcessors, resolveTag) }, projection)
            }
            else -> EmptyPoolElement
        }
    }

    private fun parseProjection(json: JsonObject): StructureProjection =
        when (json["projection"]?.jsonPrimitive?.content) {
            "terrain_matching" -> StructureProjection.TERRAIN_MATCHING
            else -> StructureProjection.RIGID
        }

    private fun parseProcessorsField(
        field: kotlinx.serialization.json.JsonElement?,
        resolveProcessors: (String) -> String?,
        resolveTag: (String) -> Set<Int>?,
    ): StructureProcessorList {
        if (field == null) return StructureProcessorList.EMPTY
        return when (field) {
            is JsonPrimitive -> {
                val name = field.content
                val json = resolveProcessors(name) ?: return StructureProcessorList.EMPTY
                parseProcessorList(json, resolveTag)
            }
            is JsonObject -> parseProcessorListObject(field, resolveTag)
            else -> StructureProcessorList.EMPTY
        }
    }

    /** Parses a standalone `data/minecraft/worldgen/processor_list/<name>.json` (`{"processors": [...]}`). */
    public fun parseProcessorList(
        json: String,
        resolveTag: (String) -> Set<Int>? = { null },
    ): StructureProcessorList = parseProcessorListObject(Json.parseToJsonElement(json).jsonObject, resolveTag)

    private fun parseProcessorListObject(
        root: JsonObject,
        resolveTag: (String) -> Set<Int>?,
    ): StructureProcessorList {
        val processors = root["processors"]?.jsonArray ?: return StructureProcessorList.EMPTY
        val rules = mutableListOf<StructureProcessorRule>()
        var rot: BlockRot? = null
        for (processor in processors) {
            val processorObj = processor.jsonObject
            when (processorObj["processor_type"]?.jsonPrimitive?.content) {
                "minecraft:rule" ->
                    for (rule in processorObj["rules"]?.jsonArray ?: JsonArray(emptyList())) {
                        rules.add(parseRule(rule.jsonObject, resolveTag))
                    }
                "minecraft:block_rot" ->
                    rot =
                        BlockRot(
                            processorObj["integrity"]?.jsonPrimitive?.content?.toFloatOrNull() ?: 1.0f,
                            processorObj["rottable_blocks"]?.jsonPrimitive?.content?.let(resolveTag),
                        )
                // Everything else is either a no-op here or belongs to a structure that does not
                // read this yet; a processor list is data, so an unknown one is skipped, not fatal.
                else -> Unit
            }
        }
        return StructureProcessorList(rules, rot)
    }

    private fun parseRule(
        json: JsonObject,
        resolveTag: (String) -> Set<Int>?,
    ): StructureProcessorRule {
        val input = parseRuleTest(json["input_predicate"]?.jsonObject, resolveTag)
        val location = parseRuleTest(json["location_predicate"]?.jsonObject, resolveTag)
        val output = json["output_state"]?.jsonObject
        return StructureProcessorRule(
            input ?: StructureRuleTest.BlockMatch(Blocks.AIR.stateId),
            location,
            output?.let(::blockStateIdOf) ?: Blocks.AIR.stateId,
        )
    }

    private fun parseRuleTest(
        json: JsonObject?,
        resolveTag: (String) -> Set<Int>?,
    ): StructureRuleTest? {
        if (json == null) return null
        return when (json["predicate_type"]?.jsonPrimitive?.content) {
            "minecraft:random_block_match" -> {
                val probability = json["probability"]?.jsonPrimitive?.doubleOrNull?.toFloat() ?: 1.0f
                StructureRuleTest.RandomBlockMatch(blockStateIdOf(json), probability)
            }
            "minecraft:block_match" -> StructureRuleTest.BlockMatch(blockStateIdOf(json))
            "minecraft:blockstate_match" ->
                StructureRuleTest.BlockStateMatch(
                    (json["block_state"] ?: json["state"])
                        ?.jsonObject
                        ?.let(::blockStateIdOf) ?: Blocks.AIR.stateId,
                )
            "minecraft:tag_match" -> {
                val tag = json["tag"]?.jsonPrimitive?.content ?: return null
                StructureRuleTest.TagMatch(resolveTag(tag) ?: emptySet())
            }
            else -> null
        }
    }

    private fun blockStateIdOf(json: JsonObject): Int {
        if ("Name" in json) {
            val block = Identifier.parse(json["Name"]?.jsonPrimitive?.content ?: "minecraft:air")
            val properties =
                json["Properties"]
                    ?.jsonObject
                    ?.mapValues { (_, value) -> value.jsonPrimitive.content }
                    .orEmpty()
            return Blocks.stateIdOf(block, properties)
                ?: Blocks.REGISTRY.byIdentifier(block)?.stateId
                ?: Blocks.AIR.stateId
        }
        val blockName = json["block"]?.jsonPrimitive?.content
        return blockName?.let { Blocks.REGISTRY.byIdentifier(Identifier.parse(it))?.stateId } ?: Blocks.AIR.stateId
    }
}
