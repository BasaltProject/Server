package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the template block processors. */
class TemplateBlockProcessorsOracle {
    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** Inputs chosen to reach every branch: the aged paths, the blackstone swaps and the drops. */
    private val inputs =
        listOf(
            "stone_bricks",
            "stone",
            "chiseled_stone_bricks",
            "stone_brick_stairs",
            "cobblestone_stairs",
            "stone_brick_slab",
            "cobblestone_wall",
            "obsidian",
            "structure_void",
            "jigsaw",
            "air",
            "netherrack",
            "cobblestone",
            "mossy_cobblestone_slab",
            "oak_slab",
        ).map(::state)

    private val worldStates = listOf("air", "stone", "lava", "chest", "bedrock", "spawner").map(::state)

    private fun fold(
        name: String,
        processor: TemplateBlockProcessor,
    ) {
        var checksum = -3750763034362895579L
        var dropped = 0
        for ((inputIndex, input) in inputs.withIndex()) {
            for ((worldIndex, world) in worldStates.withIndex()) {
                for (step in 0 until 8) {
                    val position = StructurePos(inputIndex * 7 - 13, step * 9 - 20, worldIndex * 5 + step)
                    val result = processor.process(input, world, position)
                    if (result == null) dropped++
                    checksum = (checksum xor (result ?: -1).toLong()) * 1099511628211L
                }
            }
        }
        println("ORACLE $name dropped=$dropped checksum=$checksum")
    }

    @Test
    fun dump() {
        fold("ignoreStructureBlock", TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK)
        fold("ignoreStructureAndAir", TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR)
        fold("protectFeatures", TemplateBlockProcessors.PROTECT_FEATURES)
        fold("lavaSubmerged", TemplateBlockProcessors.LAVA_SUBMERGED)
        fold("blackstoneReplace", TemplateBlockProcessors.BLACKSTONE_REPLACE)
        fold("ageNone", TemplateBlockProcessors.age(0.0f))
        fold("ageHalf", TemplateBlockProcessors.age(0.5f))
        fold("ageAll", TemplateBlockProcessors.age(1.0f))
        fold(
            "chain",
            TemplateBlockProcessorChain(
                listOf(
                    TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR,
                    TemplateBlockProcessors.PROTECT_FEATURES,
                    TemplateBlockProcessors.BLACKSTONE_REPLACE,
                    TemplateBlockProcessors.age(0.5f),
                    TemplateBlockProcessors.LAVA_SUBMERGED,
                ),
            ),
        )
    }
}
