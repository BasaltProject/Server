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
 * assembly algorithm read in full from the decompiled `Placer.tryPlacingChildren`.
 *
 * Collision tracks the same nested free-space contexts as vanilla. JigsawPlacement only creates
 * and subtracts axis-aligned piece AABBs, then asks whether a deflated candidate AABB remains in
 * that space. Inclusive integer boxes plus a list of subtracted boxes are equivalent for that
 * query, including concave gaps and connectors which attach inside their source piece. Jigsaw
 * front/top orientation, aligned-vs-rollable joints, selection-priority sorting and the
 * placement-priority queue are also modeled. The expansion hack used by some village pools is
 * still outside this class; Bastion explicitly disables that path.
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
        maxVerticalDistanceFromCenter: Int = Int.MAX_VALUE,
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
                if (maxVerticalDistanceFromCenter ==
                    Int.MAX_VALUE
                ) {
                    Int.MIN_VALUE
                } else {
                    bottomY - maxVerticalDistanceFromCenter
                },
                centerZ - maxDistanceFromCenter,
                centerX + maxDistanceFromCenter,
                if (maxVerticalDistanceFromCenter ==
                    Int.MAX_VALUE
                ) {
                    Int.MAX_VALUE
                } else {
                    bottomY + maxVerticalDistanceFromCenter
                },
                centerZ + maxDistanceFromCenter,
            )

        val globalFree = CollisionSpace(allowedRegion, mutableListOf(centerBox))
        val queue = mutableListOf(QueuedPiece(pieces[0], globalFree, 0, Int.MAX_VALUE, 0))
        var insertionOrder = 1
        while (queue.isNotEmpty()) {
            val nextIndex =
                queue.indices.maxWithOrNull(compareBy<Int> { queue[it].priority }.thenBy { -queue[it].order }) ?: 0
            val (sourcePiece, sourceFree, depth) = queue.removeAt(nextIndex)
            insertionOrder =
                tryPlacingChildren(
                    sourcePiece,
                    depth,
                    maxDepth,
                    random,
                    pieces,
                    sourceFree,
                    queue,
                    insertionOrder,
                )
        }
        return pieces
    }

    private fun tryPlacingChildren(
        sourcePiece: PlacedStructurePiece,
        depth: Int,
        maxDepth: Int,
        random: WorldgenRandom,
        pieces: MutableList<PlacedStructurePiece>,
        contextFree: CollisionSpace,
        queue: MutableList<QueuedPiece>,
        insertionOrder: Int,
    ): Int {
        var nextInsertionOrder = insertionOrder
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
        var insideSourceFree: CollisionSpace? = null

        for (sourceJigsaw in sourceJigsaws) {
            val targetJigsawPos = sourceJigsaw.front.offset(sourceJigsaw.pos)
            val sourceJigsawLocalY = sourceJigsaw.pos.y - sourceBB.minY
            val childrenFree =
                if (sourceBB.isInside(targetJigsawPos)) {
                    insideSourceFree ?: CollisionSpace(sourceBB, mutableListOf()).also { insideSourceFree = it }
                } else {
                    contextFree
                }

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

                        if (childrenFree.canFit(targetBB)) {
                            childrenFree.occupy(targetBB)
                            val targetPiece =
                                PlacedStructurePiece(targetElement, targetBoxPosition, targetRotation, targetBB)
                            pieces.add(targetPiece)
                            if (depth + 1 <= maxDepth) {
                                queue +=
                                    QueuedPiece(
                                        targetPiece,
                                        childrenFree,
                                        depth + 1,
                                        sourceJigsaw.placementPriority,
                                        nextInsertionOrder++,
                                    )
                            }
                            placed = true
                            break@outer
                        }
                    }
                }
            }
            if (!placed) continue
        }
        return nextInsertionOrder
    }

    /** // parity: JigsawBlock.canAttach. */
    private fun canAttach(
        source: PlacedJigsaw,
        target: PlacedJigsaw,
    ): Boolean =
        source.front == target.front.opposite() &&
            (source.rollable || source.top == target.top) &&
            source.target == target.name

    private data class QueuedPiece(
        val piece: PlacedStructurePiece,
        val free: CollisionSpace,
        val depth: Int,
        val priority: Int,
        val order: Int,
    )

    private class CollisionSpace(
        private val bounds: StructureBoundingBox,
        private val occupied: MutableList<StructureBoundingBox>,
    ) {
        fun canFit(candidate: StructureBoundingBox): Boolean =
            bounds.containsFully(candidate) && occupied.none { it.intersects(candidate) }

        fun occupy(box: StructureBoundingBox) {
            occupied += box
        }
    }
}
