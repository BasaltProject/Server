package org.basaltmc.server.runtime.world

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.Items
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.roundToInt

/** Exact 26.2 `mob_spawn_equipment` enchantment-provider evaluator. */
internal class SpawnEquipmentEnchantments(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Spawn equipment"),
) {
    private val definitions: Map<Identifier, Definition> = serverJar?.let(::loadDefinitions).orEmpty()
    private val itemTags: Map<Identifier, Set<Identifier>> = serverJar?.let(::loadItemTags).orEmpty()
    private val enchantmentTags: Map<Identifier, Set<Identifier>> = serverJar?.let(::loadEnchantmentTags).orEmpty()

    /** Applies the provider to [stack], consuming every draw even when no enchantment is available. */
    fun enchant(
        stack: NbtCompound,
        random: BasaltRandom,
        specialMultiplier: Float,
    ) {
        // parity: Mth.randomBetweenInclusive always draws, including a zero-width range.
        val cost = MIN_COST + random.nextInt((specialMultiplier * MAX_COST_SPAN).toInt() + 1)
        val item = Identifier.parse(stack.getString("id"))
        val enchantability = VanillaItemEnchantability.valueOf(item.toString()) ?: return
        var effectiveCost =
            cost +
                1 +
                random.nextInt(enchantability / 4 + 1) +
                random.nextInt(enchantability / 4 + 1)
        val randomSpan = (random.nextFloat() + random.nextFloat() - 1f) * COST_RANDOM_SPAN
        effectiveCost = (effectiveCost + effectiveCost * randomSpan).roundToInt().coerceAtLeast(1)

        val candidates = available(item, effectiveCost).toMutableList()
        if (candidates.isEmpty()) return
        val selected = mutableListOf(weightedPick(candidates, random))
        while (random.nextInt(ADDITIONAL_ENCHANTMENT_BOUND) <= effectiveCost) {
            val previous = selected.last()
            candidates.removeAll { !compatible(previous.definition, it.definition) }
            if (candidates.isEmpty()) break
            selected += weightedPick(candidates, random)
            effectiveCost /= 2
        }

        val components = stack.getCompound("components")
        val levels = components.getCompound(ENCHANTMENTS_COMPONENT)
        for (candidate in selected) {
            val key = candidate.id.toString()
            levels.putInt(key, maxOf(levels.getInt(key), candidate.level))
        }
        components[ENCHANTMENTS_COMPONENT] = levels
        stack["components"] = components
    }

    private fun available(
        item: Identifier,
        cost: Int,
    ): List<Candidate> =
        enchantmentTags[ON_MOB_SPAWN_EQUIPMENT]
            .orEmpty()
            .mapNotNull { id ->
                val definition = definitions[id] ?: return@mapNotNull null
                if (!definition.primaryItems.matches(item, itemTags)) return@mapNotNull null
                for (level in definition.maxLevel downTo 1) {
                    if (cost in definition.minCost.at(level)..definition.maxCost.at(level)) {
                        return@mapNotNull Candidate(id, definition, level)
                    }
                }
                null
            }

    private fun compatible(
        first: Definition,
        second: Definition,
    ): Boolean =
        first.id != second.id &&
            !first.exclusive.matches(second.id, enchantmentTags) &&
            !second.exclusive.matches(first.id, enchantmentTags)

    private fun weightedPick(
        candidates: List<Candidate>,
        random: BasaltRandom,
    ): Candidate {
        var index = random.nextInt(candidates.sumOf { it.definition.weight })
        for (candidate in candidates) {
            index -= candidate.definition.weight
            if (index < 0) return candidate
        }
        error("Positive enchantment weight total produced no candidate")
    }

    private fun loadDefinitions(serverJar: ServerJarAssets): Map<Identifier, Definition> {
        val definitions = linkedMapOf<Identifier, Definition>()
        for (entry in serverJar.entriesUnder(ENCHANTMENT_PREFIX).sorted()) {
            if (!entry.endsWith(".json")) continue
            val id = Identifier.vanilla(entry.removePrefix(ENCHANTMENT_PREFIX).removeSuffix(".json"))
            runCatching {
                val json = Json.parseToJsonElement(serverJar.readText(entry)).jsonObject
                val supported = Selector.parse(json.getValue("supported_items").jsonPrimitive.content)
                definitions[id] =
                    Definition(
                        id = id,
                        primaryItems =
                            json["primary_items"]
                                ?.jsonPrimitive
                                ?.content
                                ?.let(Selector::parse)
                                ?: supported,
                        weight = json.int("weight"),
                        maxLevel = json.int("max_level"),
                        minCost = Cost.parse(json.getValue("min_cost").jsonObject),
                        maxCost = Cost.parse(json.getValue("max_cost").jsonObject),
                        exclusive =
                            json["exclusive_set"]
                                ?.jsonPrimitive
                                ?.content
                                ?.let(Selector::parse)
                                ?: Selector.EMPTY,
                    )
            }.onFailure { logger.warn { "Skipped spawn enchantment $id: ${it.message}" } }
        }
        return definitions
    }

    private fun loadItemTags(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> =
        loadTags(serverJar, ITEM_TAG_PREFIX) { Items.REGISTRY.byIdentifier(it) != null }

    private fun loadEnchantmentTags(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> =
        loadTags(serverJar, ENCHANTMENT_TAG_PREFIX) { it in definitions }

    private fun loadTags(
        serverJar: ServerJarAssets,
        prefix: String,
        exists: (Identifier) -> Boolean,
    ): Map<Identifier, Set<Identifier>> {
        val files = linkedMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(prefix).sorted()) {
            if (!entry.endsWith(".json")) continue
            val id = Identifier.vanilla(entry.removePrefix(prefix).removeSuffix(".json"))
            runCatching { files[id] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.warn { "Skipped spawn-equipment tag $id: ${it.message}" } }
        }
        return runCatching { TagSet.resolve(files, exists) }
            .getOrElse {
                logger.warn { "Spawn-equipment tags could not be resolved: ${it.message}" }
                emptyMap()
            }
    }

    private data class Definition(
        val id: Identifier,
        val primaryItems: Selector,
        val weight: Int,
        val maxLevel: Int,
        val minCost: Cost,
        val maxCost: Cost,
        val exclusive: Selector,
    )

    private data class Candidate(
        val id: Identifier,
        val definition: Definition,
        val level: Int,
    )

    private data class Cost(
        val base: Int,
        val perLevel: Int,
    ) {
        fun at(level: Int): Int = base + perLevel * (level - 1)

        companion object {
            fun parse(json: JsonObject): Cost = Cost(json.int("base"), json.int("per_level_above_first"))
        }
    }

    private data class Selector(
        val id: Identifier?,
        val tag: Identifier?,
    ) {
        fun matches(
            candidate: Identifier,
            tags: Map<Identifier, Set<Identifier>>,
        ): Boolean = id == candidate || tag?.let { candidate in tags[it].orEmpty() } == true

        companion object {
            val EMPTY = Selector(null, null)

            fun parse(value: String): Selector =
                if (value.startsWith('#')) {
                    Selector(null, Identifier.parse(value.drop(1)))
                } else {
                    Selector(Identifier.parse(value), null)
                }
        }
    }

    private companion object {
        const val ENCHANTMENT_PREFIX = "data/minecraft/enchantment/"
        const val ITEM_TAG_PREFIX = "data/minecraft/tags/item/"
        const val ENCHANTMENT_TAG_PREFIX = "data/minecraft/tags/enchantment/"
        const val ENCHANTMENTS_COMPONENT = "minecraft:enchantments"
        const val MIN_COST = 5
        const val MAX_COST_SPAN = 17
        const val COST_RANDOM_SPAN = 0.15f
        const val ADDITIONAL_ENCHANTMENT_BOUND = 50
        val ON_MOB_SPAWN_EQUIPMENT = Identifier.vanilla("on_mob_spawn_equipment")
    }
}

private fun JsonObject.int(name: String): Int = getValue(name).jsonPrimitive.content.toInt()
