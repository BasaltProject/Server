package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate

/**
 * // parity: `FossilFeature`. A fossil is two templates at one place: the bone base and an ore overlay
 * its own `block_rot` thins. Both sit well below the placement's ground, which is why this scans its
 * whole footprint for the lowest ocean floor rather than trusting the one column it was handed.
 */
public object FossilFeature {
    /** One of the two fossil features, which differ only in their overlay's processors. */
    public class Spec(
        public val structures: List<String>,
        public val overlays: List<String>,
        public val fossilProcessors: StructureProcessorList,
        public val overlayProcessors: StructureProcessorList,
        public val maxEmptyCornersAllowed: Int,
    )

    /**
     * Places one fossil at [origin], or reports false without writing. The draw order is the jar's and not
     * the obvious one: rotation, then structure index, then burial depth.
     */
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: StructurePos,
        spec: Spec,
        templates: TemplateLookup,
        oceanFloorAt: (x: Int, z: Int) -> Int?,
    ): Boolean {
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val index = random.nextInt(spec.structures.size)
        val base = templates(spec.structures[index]) ?: return false
        val overlay = templates(spec.overlays[index]) ?: return false

        val size = rotatedSize(base, rotation)
        val lowCorner = StructurePos(origin.x - size.x / 2, origin.y, origin.z - size.z / 2)
        var lowestSurfaceY = origin.y
        for (scanX in 0 until size.x) {
            for (scanZ in 0 until size.z) {
                val height = oceanFloorAt(lowCorner.x + scanX, lowCorner.z + scanZ) ?: continue
                if (height < lowestSurfaceY) lowestSurfaceY = height
            }
        }
        val targetY = maxOf(lowestSurfaceY - BURIAL_DEPTH - random.nextInt(BURIAL_JITTER), WorldHeight.MIN_Y + 10)
        val targetPos = zeroPositionWithTransform(lowCorner.copy(y = targetY), rotation, base)

        val pivot = StructurePos(0, 0, 0)
        val box = base.getBoundingBox(targetPos, rotation, pivot, StructureMirror.NONE)
        if (emptyCorners(chunk, box) > spec.maxEmptyCornersAllowed) return false

        // parity: the level bounding box is the chunk grown by 16 on each side, so a fossil that
        // starts here may reach a neighbour; what lands there is deferred like any other write.
        val clip =
            StructureBoundingBox(
                chunk.chunkX * 16 - 16,
                WorldHeight.MIN_Y,
                chunk.chunkZ * 16 - 16,
                chunk.chunkX * 16 + 31,
                WorldHeight.MAX_Y,
                chunk.chunkZ * 16 + 31,
            )
        base.placeInto(
            chunk = chunk,
            position = targetPos,
            rotation = rotation,
            mirror = StructureMirror.NONE,
            pivot = pivot,
            clipTo = clip,
            processors = spec.fossilProcessors,
            // parity: the feature calls `setRandom(random)`, so the palette pick and the rot draw
            // from this stream in template order rather than from a fresh source per position.
            settingsRandom = random,
        )
        overlay.placeInto(
            chunk = chunk,
            position = targetPos,
            rotation = rotation,
            mirror = StructureMirror.NONE,
            pivot = pivot,
            clipTo = clip,
            processors = spec.overlayProcessors,
            settingsRandom = random,
        )
        return true
    }

    /** // parity: StructureTemplate.getSize(rotation), which swaps X and Z on a quarter turn. */
    private fun rotatedSize(
        template: StructureTemplate,
        rotation: StructureRotation,
    ): StructurePos =
        if (rotation == StructureRotation.CLOCKWISE_90 || rotation == StructureRotation.COUNTERCLOCKWISE_90) {
            StructurePos(template.size.z, template.size.y, template.size.x)
        } else {
            template.size
        }

    /** // parity: getZeroPositionWithTransform with Mirror.NONE, which only ever offsets. */
    private fun zeroPositionWithTransform(
        zeroPos: StructurePos,
        rotation: StructureRotation,
        template: StructureTemplate,
    ): StructurePos {
        val sizeX = template.size.x - 1
        val sizeZ = template.size.z - 1
        return when (rotation) {
            StructureRotation.COUNTERCLOCKWISE_90 -> StructurePos(zeroPos.x, zeroPos.y, zeroPos.z + sizeX)
            StructureRotation.CLOCKWISE_90 -> StructurePos(zeroPos.x + sizeZ, zeroPos.y, zeroPos.z)
            StructureRotation.CLOCKWISE_180 -> StructurePos(zeroPos.x + sizeX, zeroPos.y, zeroPos.z + sizeZ)
            StructureRotation.NONE -> zeroPos
        }
    }

    /**
     * // parity: countEmptyCorners over `BoundingBox.forAllCorners`, the eight corners of the box
     * and nothing between them. Air, lava and water each count as empty.
     */
    private fun emptyCorners(
        chunk: Chunk,
        box: StructureBoundingBox,
    ): Int {
        var count = 0
        for (x in intArrayOf(box.maxX, box.minX)) {
            for (y in intArrayOf(box.maxY, box.minY)) {
                for (z in intArrayOf(box.maxZ, box.minZ)) {
                    if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        count++
                        continue
                    }
                    val state = chunk.getDecorationBlockStateId(x, y, z)
                    val path = Blocks.byStateId(state)?.block?.path
                    if (Blocks.isAir(state) || path == "lava" || path == "water") count++
                }
            }
        }
        return count
    }

    private const val BURIAL_DEPTH = 15
    private const val BURIAL_JITTER = 10
}

/** Resolves a structure path such as `fossil/spine_1` to its template. */
public typealias TemplateLookup = (String) -> StructureTemplate?
