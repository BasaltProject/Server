package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.GeneratedBlockWritePolicy

/**
 * // parity: OreFeature re-tests its target predicate against whatever block is really there when it
 * writes. A deferred write across a chunk edge must too: guarding on the block seen at defer time
 * drops every position a neighbour touched first, and those are all on the border.
 */
public object OreBlockRules {
    public fun canReplaceDeferred(
        policy: GeneratedBlockWritePolicy,
        actualStateId: Int,
    ): Boolean {
        val path = Blocks.byStateId(actualStateId)?.block?.path ?: return false
        return when (policy) {
            GeneratedBlockWritePolicy.ORE_BASE_STONE -> path in BASE_STONE
            GeneratedBlockWritePolicy.ORE_STONE -> path in STONE_REPLACEABLES
            GeneratedBlockWritePolicy.ORE_DEEPSLATE -> path in DEEPSLATE_REPLACEABLES
            else -> false
        }
    }

    internal fun policyFor(replaceable: Set<String>): GeneratedBlockWritePolicy =
        when {
            replaceable.size == DEEPSLATE_REPLACEABLES.size && replaceable.containsAll(DEEPSLATE_REPLACEABLES) ->
                GeneratedBlockWritePolicy.ORE_DEEPSLATE
            replaceable.size == STONE_REPLACEABLES.size && replaceable.containsAll(STONE_REPLACEABLES) ->
                GeneratedBlockWritePolicy.ORE_STONE
            replaceable.containsAll(BASE_STONE) -> GeneratedBlockWritePolicy.ORE_BASE_STONE
            // Disks and anything else keep the strict guard they had.
            else -> GeneratedBlockWritePolicy.EXACT
        }

    private val BASE_STONE = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")
    private val STONE_REPLACEABLES = setOf("stone", "granite", "diorite", "andesite")
    private val DEEPSLATE_REPLACEABLES = setOf("deepslate", "tuff")
}
