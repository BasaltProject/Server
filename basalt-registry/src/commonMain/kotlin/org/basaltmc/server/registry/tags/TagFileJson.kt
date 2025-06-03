package org.basaltmc.server.registry.tags

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.wire.common.Identifier

/** Thrown for tag file JSON this parser doesn't understand: an unsupported shape, not malformed JSON. */
public class TagParseException(
    message: String,
) : Exception(message)

/**
 * Parses the real vanilla tag file JSON schema (`{"replace": bool, "values": [...]}`) into
 * [TagFile]. Each value is either a plain string (an element, or `#id` for a tag reference, always
 * required) or `{"id": "...", "required": bool}` (same `#`-prefix rule for [TagEntry.id], `required`
 * defaults to true). Matches vanilla's `Codec.either(TAG_OR_ELEMENT_ID, FULL_CODEC)`.
 */
public object TagFileJson {
    public fun parse(json: String): TagFile = parse(Json.parseToJsonElement(json) as JsonObject)

    public fun parse(root: JsonObject): TagFile =
        TagFile(
            entries = (root.getValue("values") as JsonArray).map(::parseEntry),
            replace = (root["replace"] as? JsonPrimitive)?.content?.toBoolean() ?: false,
        )

    private fun parseEntry(element: JsonElement): TagEntry =
        when (element) {
            is JsonPrimitive -> idOrTag(element.content, required = true)
            is JsonObject ->
                idOrTag(
                    raw = element.getValue("id").jsonPrimitive.content,
                    required = (element["required"] as? JsonPrimitive)?.content?.toBoolean() ?: true,
                )
            else -> throw TagParseException("Unsupported tag entry JSON: $element")
        }

    private fun idOrTag(
        raw: String,
        required: Boolean,
    ): TagEntry =
        if (raw.startsWith("#")) {
            TagEntry(Identifier.parse(raw.substring(1)), isTag = true, required = required)
        } else {
            TagEntry(Identifier.parse(raw), isTag = false, required = required)
        }
}
