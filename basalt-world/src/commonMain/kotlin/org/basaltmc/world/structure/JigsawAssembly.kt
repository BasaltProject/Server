package org.basaltmc.world.structure

import org.basaltmc.random.WorldgenRandom

/** A single assembled piece: which NBT element, where, and how it's rotated. */
public data class PlacedStructurePiece(
    public val element: StructurePoolElement,
    public val position: StructurePos,
    public val rotation: StructureRotation,
    public val boundingBox: StructureBoundingBox,
)

/**
 * // parity: net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement, the BFS jigsaw
 * assembly algorithm read in full from the decompiled `Placer.tryPlacingChildren`. Two documented
 * simplifications, both because the supporting real-vanilla infrastructure doesn't exist in this
 * project yet:
 *
 * - **Collision uses plain integer bounding-box intersection**, not real vanilla's exact
 *   `VoxelShape` boolean-region tracking (`Shapes.join`/`joinIsNotEmpty` over the accumulated
 *   "free space"). For convex pieces (every real Plains village piece is a simple box) this gives
 *   the same accept/reject outcome; it can diverge in concave free-space gaps between several
 *   already-placed pieces, which real Plains village layouts essentially never produce.
 * - **`JointType`/top-facing is not checked**: this project only extracts a jigsaw block's front
 *   direction from its `orientation` property (see [StructureTemplate]'s own KDoc for why), not
 *   its top direction, so `canAttach` always behaves as if every joint were `rollable` (real
 *   vanilla's own behavior for vertical connections; only some horizontal ones are normally
 *   `aligned`). `placementPriority`/`selectionPriority` are read but not used for ordering either,
 *   since every sampled real Plains village jigsaw block leaves both at their default of 0.
 *
 * Every real Plains village pool element uses `projection: rigid`, so the `sourceRigid &&
 * targetRigid` path (a plain Y delta, no heightmap query) is what actually runs in practice; the
 * `terrain_matching` path (calling [heightAt]) is wired for completeness but effectively untested
 * against real data.
 *
 * **A real bug found and fixed after real gameplay reproduced a CPU-pegged hang**: this class
 * originally had no equivalent of real vanilla's own outer bound on the jigsaw "free space"
 * (`JigsawPlacement.addPieces`'s own `AABB` capped to `maxDistanceFromCenter`, which the free
 * region a candidate piece must fit *entirely* inside is intersected against before anything else
 * runs). Without it, nothing stopped a chain of connections from sprawling outward indefinitely
 * (bounded by [assemble]'s own `maxDepth`, but real pools branch widely enough that "bounded by
 * depth" was still large: a stress test across a wide scattered chunk area reliably produced
 * assemblies with piece counts far past what a real village ever has, and since collision
 * checking is `O(n)` per candidate against every already-placed piece, an `O(n)`-piece runaway
 * assembly is `O(n^2)` overall). [maxDistanceFromCenter] is now enforced exactly where real
 * vanilla enforces it: a candidate is rejected unless its bounding box lies entirely within the
 * square centered on the start piece, independent of (and checked before) the collision test.
 */
public class JigsawAssembly(
    private val poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
    private val heightAt: (x: Int, z: Int) -> Int,
) {
    public fun assemble(
        startPoolName: String,
        startPosition: StructurePos,
        maxDepth: Int,
        random: WorldgenRandom,
        projectStartToHeightmap: Boolean,
        maxDistanceFromCenter: Int,
    ): List<PlacedStructurePiece> {
        val startPool = poolLookup(startPoolName) ?: return emptyList()
        val centerRotation = StructureRotations.random(random)
        val centerElement = startPool.getRandomTemplate(random)
        if (centerElement === EmptyPoolElement) return emptyList()

        val rawBox = centerElement.getBoundingBox(templateLoader, startPosition, centerRotation)
        val centerX = (rawBox.maxX + rawBox.minX) / 2
        val centerZ = (rawBox.maxZ + rawBox.minZ) / 2
        val bottomY = if (projectStartToHeightmap) heightAt(centerX, centerZ) else startPosition.y
        val oldGroundY = rawBox.minY + centerElement.groundLevelDelta
        val moveY = bottomY - oldGroundY
        val centerPosition = StructurePos(startPosition.x, startPosition.y + moveY, startPosition.z)
        val centerBox = rawBox.move(StructurePos(0, moveY, 0))

        val pieces = mutableListOf(PlacedStructurePiece(centerElement, centerPosition, centerRotation, centerBox))
        if (maxDepth <= 0) return pieces

        // parity: JigsawPlacement.addPieces's own bounding AABB, capped to maxDistanceFromCenter
        // around the center piece; see this class's own KDoc for why this bound matters.
        val allowedRegion =
            StructureBoundingBox(
                centerX - maxDistanceFromCenter,
                Int.MIN_VALUE,
                centerZ - maxDistanceFromCenter,
                centerX + maxDistanceFromCenter,
                Int.MAX_VALUE,
                centerZ + maxDistanceFromCenter,
            )

        val placedBoxes = mutableListOf(centerBox)
        val queue = ArrayDeque<Pair<PlacedStructurePiece, Int>>()
        queue.add(pieces[0] to 0)
        while (queue.isNotEmpty()) {
            val (sourcePiece, depth) = queue.removeFirst()
            tryPlacingChildren(sourcePiece, depth, maxDepth, random, pieces, placedBoxes, queue, allowedRegion)
        }
        return pieces
    }

    private fun tryPlacingChildren(
        sourcePiece: PlacedStructurePiece,
        depth: Int,
        maxDepth: Int,
        random: WorldgenRandom,
        pieces: MutableList<PlacedStructurePiece>,
        placedBoxes: MutableList<StructureBoundingBox>,
        queue: ArrayDeque<Pair<PlacedStructurePiece, Int>>,
        allowedRegion: StructureBoundingBox,
    ) {
        val sourceElement = sourcePiece.element
        val sourceRigid = sourceElement.projection == StructureProjection.RIGID
        val sourceBB = sourcePiece.boundingBox
        val sourceJigsaws =
            sourceElement.getShuffledJigsawBlocks(
                templateLoader,
                sourcePiece.position,
                sourcePiece.rotation,
                random,
            )

        for (sourceJigsaw in sourceJigsaws) {
            val targetJigsawPos = sourceJigsaw.front.offset(sourceJigsaw.pos)
            val sourceJigsawLocalY = sourceJigsaw.pos.y - sourceBB.minY

            val targetPool = poolLookup(sourceJigsaw.pool)
            if (targetPool == null || targetPool.size() == 0) continue
            val fallbackPool = poolLookup(targetPool.fallback)

            val targetPieces = mutableListOf<StructurePoolElement>()
            if (depth != maxDepth) targetPieces.addAll(targetPool.getShuffledTemplates(random))
            if (fallbackPool != null) targetPieces.addAll(fallbackPool.getShuffledTemplates(random))

            var placed = false
            outer@ for (targetElement in targetPieces) {
                if (targetElement === EmptyPoolElement) break

                for (targetRotation in StructureRotations.shuffled(random)) {
                    val targetJigsaws =
                        targetElement.getShuffledJigsawBlocks(
                            templateLoader,
                            StructurePos(0, 0, 0),
                            targetRotation,
                            random,
                        )
                    val targetRigid = targetElement.projection == StructureProjection.RIGID

                    for (targetJigsaw in targetJigsaws) {
                        if (!canAttach(sourceJigsaw, targetJigsaw)) continue

                        val rawTargetBoxPos = targetJigsawPos - targetJigsaw.pos
                        val rawTargetBB = targetElement.getBoundingBox(templateLoader, rawTargetBoxPos, targetRotation)
                        val targetJigsawLocalY = targetJigsaw.pos.y
                        val deltaY = sourceJigsawLocalY - targetJigsawLocalY + sourceJigsaw.front.dy
                        val targetBoxY =
                            if (sourceRigid && targetRigid) {
                                sourceBB.minY + deltaY
                            } else {
                                heightAt(sourceJigsaw.pos.x, sourceJigsaw.pos.z) - targetJigsawLocalY
                            }
                        val yOffset = targetBoxY - rawTargetBB.minY
                        val targetBB = rawTargetBB.move(StructurePos(0, yOffset, 0))
                        val targetBoxPosition = rawTargetBoxPos + StructurePos(0, yOffset, 0)

                        if (allowedRegion.containsFully(targetBB) && placedBoxes.none { it.intersects(targetBB) }) {
                            placedBoxes.add(targetBB)
                            val targetPiece =
                                PlacedStructurePiece(targetElement, targetBoxPosition, targetRotation, targetBB)
                            pieces.add(targetPiece)
                            if (depth + 1 <= maxDepth) queue.add(targetPiece to depth + 1)
                            placed = true
                            break@outer
                        }
                    }
                }
            }
            if (!placed) continue
        }
    }

    /** // parity: JigsawBlock.canAttach, minus the top-facing/JointType check (see this class's own KDoc). */
    private fun canAttach(
        source: PlacedJigsaw,
        target: PlacedJigsaw,
    ): Boolean = source.front == target.front.opposite() && source.target == target.name
}
