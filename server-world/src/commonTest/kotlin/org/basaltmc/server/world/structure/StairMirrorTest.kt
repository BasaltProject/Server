package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * A corner stair does not follow its facing under a mirror, and the two mirrors do not agree on
 * which halves swap. Getting it wrong leaves a roof that looks right until you walk the corner,
 * which is exactly the kind of thing nobody re-derives once it is written.
 */
class StairMirrorTest {
    private val template = StructureTemplate()

    private fun stair(
        facing: String,
        shape: String,
    ): Pair<Int, Map<String, String>> {
        val properties =
            mapOf("facing" to facing, "half" to "bottom", "shape" to shape, "waterlogged" to "false")
        val id =
            Blocks.stateIdOf(Identifier.vanilla("spruce_stairs"), properties)
                ?: error("no spruce stairs with $properties")
        return id to properties
    }

    private fun mirrored(
        facing: String,
        shape: String,
        mirror: StructureMirror,
    ): Map<String, String> {
        val (id, properties) = stair(facing, shape)
        val out = template.transformedStateId(id, properties, mirror, StructureRotation.NONE)
        return Blocks.byStateId(out)?.properties ?: error("no state for $out")
    }

    @Test
    fun `left-right swaps both pairs when the stair faces along z`() {
        val outer = mirrored("north", "outer_left", StructureMirror.LEFT_RIGHT)
        assertEquals("south", outer["facing"])
        assertEquals("outer_right", outer["shape"])

        val inner = mirrored("south", "inner_right", StructureMirror.LEFT_RIGHT)
        assertEquals("north", inner["facing"])
        assertEquals("inner_left", inner["shape"])
    }

    @Test
    fun `front-back swaps only the outer pair`() {
        val outer = mirrored("east", "outer_left", StructureMirror.FRONT_BACK)
        assertEquals("west", outer["facing"])
        assertEquals("outer_right", outer["shape"])

        // The jar leaves inner_left alone here, and that asymmetry is deliberate.
        val inner = mirrored("west", "inner_left", StructureMirror.FRONT_BACK)
        assertEquals("east", inner["facing"])
        assertEquals("inner_left", inner["shape"])
    }

    @Test
    fun `a mirror across the other axis leaves the stair untouched`() {
        val across = mirrored("east", "outer_left", StructureMirror.LEFT_RIGHT)
        assertEquals("east", across["facing"])
        assertEquals("outer_left", across["shape"])
    }

    @Test
    fun `a straight stair keeps its shape and only turns`() {
        val straight = mirrored("north", "straight", StructureMirror.LEFT_RIGHT)
        assertEquals("south", straight["facing"])
        assertEquals("straight", straight["shape"])
    }

    @Test
    fun `rotation turns a corner stair without touching its shape`() {
        val (id, properties) = stair("north", "outer_left")
        val out = template.transformedStateId(id, properties, StructureMirror.NONE, StructureRotation.CLOCKWISE_90)
        val result = Blocks.byStateId(out)?.properties ?: error("no state")
        assertEquals("east", result["facing"])
        assertEquals("outer_left", result["shape"])
    }

    @Test
    fun `a door swaps its hinge under any mirror and a chest keeps its half`() {
        val doorProperties =
            mapOf(
                "facing" to "east",
                "half" to "lower",
                "hinge" to "left",
                "open" to "false",
                "powered" to "false",
            )
        val door =
            Blocks.stateIdOf(Identifier.vanilla("oak_door"), doorProperties) ?: error("no oak door")
        // parity: the hinge cycles even when the mirror does not touch this facing's axis.
        val across =
            Blocks
                .byStateId(
                    template.transformedStateId(
                        door,
                        doorProperties,
                        StructureMirror.LEFT_RIGHT,
                        StructureRotation.NONE,
                    ),
                )?.properties ?: error("no state")
        assertEquals("east", across["facing"], "left-right does not turn an east-facing door")
        assertEquals("right", across["hinge"], "but it still swaps the hinge")

        // parity: ChestBlock.mirror only rotates the facing; `type` is left alone.
        val chestProperties = mapOf("facing" to "north", "type" to "left", "waterlogged" to "false")
        val chest = Blocks.stateIdOf(Identifier.vanilla("chest"), chestProperties) ?: error("no chest")
        val mirrored =
            Blocks
                .byStateId(
                    template.transformedStateId(
                        chest,
                        chestProperties,
                        StructureMirror.LEFT_RIGHT,
                        StructureRotation.NONE,
                    ),
                )?.properties ?: error("no state")
        assertEquals("south", mirrored["facing"])
        assertEquals("left", mirrored["type"], "a chest half is not swapped by a mirror")
    }
}
