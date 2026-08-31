package org.basaltmc.server.world.portal

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the portal frame search. */
class PortalShapeOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A world holding one frame on [axis] with the given interior size, optionally lit and
     * optionally with one frame block knocked out.
     */
    private fun frame(
        axis: PortalAxis,
        width: Int,
        height: Int,
        lit: Boolean = false,
        broken: Boolean = false,
        litRows: Int = height,
        burning: Boolean = false,
    ): BlockStateView {
        val obsidian = stateId("obsidian")
        val portal = PortalShape.portalState(axis)
        val blocks = HashMap<Triple<Int, Int, Int>, Int>()
        val rx = axis.rightX
        val rz = axis.rightZ
        for (column in -1..width) {
            for (row in -1..height) {
                val x = 100 + rx * column
                val z = 200 + rz * column
                val y = 64 + row
                val edge = column == -1 || column == width || row == -1 || row == height
                blocks[Triple(x, y, z)] =
                    when {
                        edge -> if (broken && column == 0 && row == height) Blocks.AIR.stateId else obsidian
                        burning && column == 0 && row == 0 -> stateId("fire")
                        lit && row < litRows -> portal
                        else -> Blocks.AIR.stateId
                    }
            }
        }
        return BlockStateView { x, y, z -> blocks[Triple(x, y, z)] ?: Blocks.STONE.stateId }
    }

    private fun describe(
        name: String,
        shape: PortalShape?,
    ) {
        if (shape == null) {
            println("ORACLE $name found=0")
            return
        }
        println(
            "ORACLE $name found=1 axis=${shape.axis.name} x=${shape.bottomLeftX} y=${shape.bottomLeftY} " +
                "z=${shape.bottomLeftZ} w=${shape.width} h=${shape.height} valid=${shape.isValid} " +
                "complete=${shape.isComplete} interior=${shape.interior().size}",
        )
    }

    @Test
    fun dump() {
        for (axis in PortalAxis.entries) {
            val name = axis.name.lowercase()
            describe("${name}_any", PortalShape.findAnyShape(frame(axis, 2, 3), 100, 64, 200, -64, axis))
            describe("${name}_empty", PortalShape.findEmpty(frame(axis, 2, 3), 100, 64, 200, -64, axis))
            // Lighting the top of the frame has to find the same portal as lighting the bottom.
            describe("${name}_fromTop", PortalShape.findEmpty(frame(axis, 3, 4), 100, 66, 200, -64, axis))
            describe("${name}_lit", PortalShape.findAnyShape(frame(axis, 2, 3, lit = true), 100, 64, 200, -64, axis))
            describe(
                "${name}_partlyLit",
                PortalShape.findAnyShape(frame(axis, 2, 3, lit = true, litRows = 1), 100, 64, 200, -64, axis),
            )
            describe("${name}_broken", PortalShape.findEmpty(frame(axis, 2, 3, broken = true), 100, 64, 200, -64, axis))
            // Fire inside the frame still counts as empty, which is what relighting relies on.
            describe(
                "${name}_burning",
                PortalShape.findEmpty(frame(axis, 2, 3, burning = true), 100, 64, 200, -64, axis),
            )
            describe("${name}_tooNarrow", PortalShape.findEmpty(frame(axis, 1, 4), 100, 64, 200, -64, axis))
            describe("${name}_wide", PortalShape.findEmpty(frame(axis, 21, 21), 100, 64, 200, -64, axis))
            describe("${name}_tooWide", PortalShape.findEmpty(frame(axis, 22, 5), 100, 64, 200, -64, axis))
            describe("${name}_tooShort", PortalShape.findEmpty(frame(axis, 3, 2), 100, 64, 200, -64, axis))
            // The other axis is tried when the preferred one finds nothing.
            describe(
                "${name}_otherAxis",
                PortalShape.findEmpty(frame(axis, 2, 3), 100, 64, 200, -64, axis.other),
            )
        }
        println(
            "ORACLE portalStates x=${PortalShape.portalState(PortalAxis.X)} z=${PortalShape.portalState(PortalAxis.Z)}",
        )
        val portalX = PortalShape.portalState(PortalAxis.X)
        println(
            "ORACLE axisOf portal=${PortalShape.axisOf(portalX)?.name} " +
                "stone=${PortalShape.axisOf(Blocks.STONE.stateId)?.name ?: "null"} " +
                "isPortal=${PortalShape.isPortal(portalX)}",
        )
    }
}
