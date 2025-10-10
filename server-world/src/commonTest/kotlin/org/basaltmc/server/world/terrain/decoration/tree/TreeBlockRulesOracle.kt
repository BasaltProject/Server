package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the tree block rules, over every block state rather than a
 * sample: these are tag membership questions, so a missing entry shows up in one state out of
 * thirty thousand and nowhere else.
 */
class TreeBlockRulesOracle {
    private fun fold(
        name: String,
        of: (Int) -> Long,
    ) {
        var checksum = -3750763034362895579L
        var hits = 0
        for (stateId in 0 until Blocks.stateCount) {
            val value = of(stateId)
            checksum = (checksum xor value) * 1099511628211L
            if (value != 0L) hits++
        }
        println("ORACLE $name=$checksum hits=$hits")
    }

    @Test
    fun dump() {
        fold("isLog") { if (TreeBlockRules.isLogState(it)) 1L else 0L }
        fold("isValidTree") { if (TreeBlockRules.isValidTreeState(it)) 1L else 0L }
        println("ORACLE logPaths=" + TreeBlockRules.LOG_BLOCK_PATHS.sorted().joinToString(","))

        val generated =
            listOf("oak_log", "oak_leaves", "mangrove_roots", "muddy_mangrove_roots", "stone")
                .mapNotNull { path -> Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId }
        for ((index, gen) in generated.withIndex()) {
            fold("canReplace$index") { actual -> if (TreeBlockRules.canReplaceDeferred(actual, gen)) 1L else 0L }
            fold("resolve$index") { actual -> TreeBlockRules.resolveDeferredState(actual, gen).toLong() }
        }
    }
}
