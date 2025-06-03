package org.basaltmc.server.world.structure

import org.basaltmc.server.random.WorldgenRandom

/** A single assembled piece: which NBT element, where, and how it's rotated. */
public data class PlacedStructurePiece(
    public val element: StructurePoolElement,
    public val position: StructurePos,
    public val rotation: StructureRotation,
    public val boundingBox: StructureBoundingBox,
    public val groundLevelDelta: Int = element.groundLevelDelta,
    public val junctions: MutableList<PlacedJigsawJunction> = mutableListOf(),
)

/** // parity: net.minecraft.world.level.levelgen.structure.pools.JigsawJunction. */
public data class PlacedJigsawJunction(
    public val sourceX: Int,
    public val sourceGroundY: Int,
    public val sourceZ: Int,
    public val deltaY: Int,
    public val destinationProjection: StructureProjection,
)

/**
 * // parity: net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement, BFS assembly as
 * in `Placer.tryPlacingChildren`, including jigsaw orientation, joint types and both priorities.
 *
 * Free space is inclusive integer boxes minus the placed ones, equivalent to vanilla's AABB
 * subtraction for the only query it makes. [maxDistanceFromCenter] is checked before collision, as
 * vanilla does: without it an assembly sprawls to `maxDepth` and hangs the generator.
 *
 * [useExpansionHack] reproduces the legacy village-only collision-box height expansion. Bastion
 * disables it. The `terrain_matching` path through [heightAt] follows WORLD_SURFACE_WG.
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
        useExpansionHack: Boolean = false,
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
                    useExpansionHack,
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
        useExpansionHack: Boolean,
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
                    val unexpandedBox =
                        targetElement.getBoundingBox(
                            templateLoader,
                            StructurePos(0, 0, 0),
                            targetRotation,
                        )
                    val expandTo =
                        if (useExpansionHack && unexpandedBox.maxY - unexpandedBox.minY + 1 <= 16) {
                            targetJigsaws.maxOfOrNull { childJigsaw ->
                                val childTarget = childJigsaw.front.offset(childJigsaw.pos)
                                if (!unexpandedBox.isInside(childTarget)) {
                                    0
                                } else {
                                    val childPool = poolLookup(childJigsaw.pool)
                                    val childFallback = childPool?.let { poolLookup(it.fallback) }
                                    maxOf(
                                        childPool?.maxHeight(templateLoader) ?: 0,
                                        childFallback?.maxHeight(templateLoader) ?: 0,
                                    )
                                }
                            } ?: 0
                        } else {
                            0
                        }

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
                        val movedTargetBB = rawTargetBB.move(StructurePos(0, yOffset, 0))
                        val targetBB =
                            if (expandTo > 0) {
                                val newSize = maxOf(expandTo + 1, movedTargetBB.maxY - movedTargetBB.minY)
                                movedTargetBB.copy(maxY = movedTargetBB.minY + newSize)
                            } else {
                                movedTargetBB
                            }
                        val targetBoxPosition = rawTargetBoxPos + StructurePos(0, yOffset, 0)

                        if (childrenFree.canFit(targetBB)) {
                            childrenFree.occupy(targetBB)
                            val sourceGroundLevelDelta = sourcePiece.groundLevelDelta
                            val targetGroundLevelDelta =
                                if (targetRigid) {
                                    sourceGroundLevelDelta - deltaY
                                } else {
                                    targetElement.groundLevelDelta
                                }
                            val targetPiece =
                                PlacedStructurePiece(
                                    targetElement,
                                    targetBoxPosition,
                                    targetRotation,
                                    targetBB,
                                    targetGroundLevelDelta,
                                )
                            val junctionY =
                                when {
                                    sourceRigid -> sourceBB.minY + sourceJigsawLocalY
                                    targetRigid -> targetBoxY + targetJigsawLocalY
                                    else -> heightAt(sourceJigsaw.pos.x, sourceJigsaw.pos.z) + deltaY / 2
                                }
                            sourcePiece.junctions +=
                                PlacedJigsawJunction(
                                    targetJigsawPos.x,
                                    junctionY - sourceJigsawLocalY + sourceGroundLevelDelta,
                                    targetJigsawPos.z,
                                    deltaY,
                                    targetElement.projection,
                                )
                            targetPiece.junctions +=
                                PlacedJigsawJunction(
                                    sourceJigsaw.pos.x,
                                    junctionY - targetJigsawLocalY + targetGroundLevelDelta,
                                    sourceJigsaw.pos.z,
                                    -deltaY,
                                    sourceElement.projection,
                                )
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
