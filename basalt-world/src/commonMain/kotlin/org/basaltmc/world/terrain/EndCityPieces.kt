package org.basaltmc.world.terrain

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureMirror
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureRotation
import org.basaltmc.world.structure.StructureTemplate
import org.basaltmc.world.structure.StructureTransform
import org.basaltmc.world.structure.TemplateLoader

/** // parity: EndCityPieces, the recursive template graph used by the End City structure. */
public object EndCityPieces {
    public data class Piece(
        public val templateName: String,
        public val position: StructurePos,
        public val rotation: StructureRotation,
        public val overwrite: Boolean,
        public val template: StructureTemplate,
        public var generationDepth: Int = 0,
    ) {
        public val boundingBox: StructureBoundingBox =
            template.getBoundingBox(position, rotation, ZERO, StructureMirror.NONE)
    }

    public fun assemble(
        templateLoader: TemplateLoader,
        origin: StructurePos,
        rotation: StructureRotation,
        random: WorldgenRandom,
    ): List<Piece> = Assembler(templateLoader, random).assemble(origin, rotation)

    private class Assembler(
        private val templateLoader: TemplateLoader,
        private val random: WorldgenRandom,
    ) {
        private var shipCreated = false

        fun assemble(
            origin: StructurePos,
            rotation: StructureRotation,
        ): List<Piece> {
            val pieces = mutableListOf<Piece>()
            var last = add(pieces, piece("base_floor", origin, rotation, overwrite = true))
            last = add(pieces, child(last, StructurePos(-1, 0, -1), "second_floor_1", rotation, overwrite = false))
            last = add(pieces, child(last, StructurePos(-1, 4, -1), "third_floor_1", rotation, overwrite = false))
            last = add(pieces, child(last, StructurePos(-1, 8, -1), "third_roof", rotation, overwrite = true))
            recursiveChildren(Generator.TOWER, 1, last, null, pieces)
            return pieces
        }

        private fun generate(
            generator: Generator,
            generationDepth: Int,
            parent: Piece,
            offset: StructurePos?,
            pieces: MutableList<Piece>,
        ): Boolean =
            when (generator) {
                Generator.HOUSE_TOWER -> generateHouseTower(generationDepth, parent, requireNotNull(offset), pieces)
                Generator.TOWER -> generateTower(generationDepth, parent, pieces)
                Generator.TOWER_BRIDGE -> generateTowerBridge(generationDepth, parent, pieces)
                Generator.FAT_TOWER -> generateFatTower(generationDepth, parent, pieces)
            }

        private fun generateHouseTower(
            generationDepth: Int,
            parent: Piece,
            offset: StructurePos,
            pieces: MutableList<Piece>,
        ): Boolean {
            if (generationDepth > MAX_GENERATION_DEPTH) return false
            val rotation = parent.rotation
            var last = add(pieces, child(parent, offset, "base_floor", rotation, overwrite = true))
            when (random.nextInt(3)) {
                0 -> add(pieces, child(last, StructurePos(-1, 4, -1), "base_roof", rotation, overwrite = true))
                1 -> {
                    last =
                        add(pieces, child(last, StructurePos(-1, 0, -1), "second_floor_2", rotation, overwrite = false))
                    last = add(pieces, child(last, StructurePos(-1, 8, -1), "second_roof", rotation, overwrite = false))
                    recursiveChildren(Generator.TOWER, generationDepth + 1, last, null, pieces)
                }
                2 -> {
                    last =
                        add(pieces, child(last, StructurePos(-1, 0, -1), "second_floor_2", rotation, overwrite = false))
                    last =
                        add(pieces, child(last, StructurePos(-1, 4, -1), "third_floor_2", rotation, overwrite = false))
                    last = add(pieces, child(last, StructurePos(-1, 8, -1), "third_roof", rotation, overwrite = true))
                    recursiveChildren(Generator.TOWER, generationDepth + 1, last, null, pieces)
                }
            }
            return true
        }

        private fun generateTower(
            generationDepth: Int,
            parent: Piece,
            pieces: MutableList<Piece>,
        ): Boolean {
            val rotation = parent.rotation
            var last =
                add(
                    pieces,
                    child(
                        parent,
                        StructurePos(3 + random.nextInt(2), -3, 3 + random.nextInt(2)),
                        "tower_base",
                        rotation,
                        overwrite = true,
                    ),
                )
            last = add(pieces, child(last, StructurePos(0, 7, 0), "tower_piece", rotation, overwrite = true))
            var bridgePiece: Piece? = if (random.nextInt(3) == 0) last else null
            val towerHeight = 1 + random.nextInt(3)
            repeat(towerHeight) { index ->
                last = add(pieces, child(last, StructurePos(0, 4, 0), "tower_piece", rotation, overwrite = true))
                if (index < towerHeight - 1 && random.nextBoolean()) bridgePiece = last
            }

            if (bridgePiece != null) {
                for ((bridgeRotation, bridgeOffset) in TOWER_BRIDGES) {
                    if (!random.nextBoolean()) continue
                    val bridgeStart =
                        add(
                            pieces,
                            child(
                                bridgePiece,
                                bridgeOffset,
                                "bridge_end",
                                rotation.rotatedBy(bridgeRotation),
                                overwrite = true,
                            ),
                        )
                    recursiveChildren(Generator.TOWER_BRIDGE, generationDepth + 1, bridgeStart, null, pieces)
                }
                add(pieces, child(last, StructurePos(-1, 4, -1), "tower_top", rotation, overwrite = true))
            } else {
                if (generationDepth != 7) {
                    return recursiveChildren(Generator.FAT_TOWER, generationDepth + 1, last, null, pieces)
                }
                add(pieces, child(last, StructurePos(-1, 4, -1), "tower_top", rotation, overwrite = true))
            }
            return true
        }

        private fun generateTowerBridge(
            generationDepth: Int,
            parent: Piece,
            pieces: MutableList<Piece>,
        ): Boolean {
            val rotation = parent.rotation
            val bridgeLength = random.nextInt(4) + 1
            var last = add(pieces, child(parent, StructurePos(0, 0, -4), "bridge_piece", rotation, overwrite = true))
            last.generationDepth = -1
            var nextY = 0
            repeat(bridgeLength) {
                if (random.nextBoolean()) {
                    last =
                        add(pieces, child(last, StructurePos(0, nextY, -4), "bridge_piece", rotation, overwrite = true))
                    nextY = 0
                } else {
                    if (random.nextBoolean()) {
                        last =
                            add(
                                pieces,
                                child(
                                    last,
                                    StructurePos(0, nextY, -4),
                                    "bridge_steep_stairs",
                                    rotation,
                                    overwrite = true,
                                ),
                            )
                    } else {
                        last =
                            add(
                                pieces,
                                child(
                                    last,
                                    StructurePos(0, nextY, -8),
                                    "bridge_gentle_stairs",
                                    rotation,
                                    overwrite = true,
                                ),
                            )
                    }
                    nextY = 4
                }
            }

            if (!shipCreated && random.nextInt(10 - generationDepth) == 0) {
                add(
                    pieces,
                    child(
                        last,
                        StructurePos(-8 + random.nextInt(8), nextY, -70 + random.nextInt(10)),
                        "ship",
                        rotation,
                        overwrite = true,
                    ),
                )
                shipCreated = true
            } else if (
                !recursiveChildren(
                    Generator.HOUSE_TOWER,
                    generationDepth + 1,
                    last,
                    StructurePos(-3, nextY + 1, -11),
                    pieces,
                )
            ) {
                return false
            }

            last =
                add(
                    pieces,
                    child(
                        last,
                        StructurePos(4, nextY, 0),
                        "bridge_end",
                        rotation.rotatedBy(StructureRotation.CLOCKWISE_180),
                        overwrite = true,
                    ),
                )
            last.generationDepth = -1
            return true
        }

        private fun generateFatTower(
            generationDepth: Int,
            parent: Piece,
            pieces: MutableList<Piece>,
        ): Boolean {
            val rotation = parent.rotation
            var last = add(pieces, child(parent, StructurePos(-3, 4, -3), "fat_tower_base", rotation, overwrite = true))
            last = add(pieces, child(last, StructurePos(0, 4, 0), "fat_tower_middle", rotation, overwrite = true))
            var index = 0
            while (index < 2 && random.nextInt(3) != 0) {
                last = add(pieces, child(last, StructurePos(0, 8, 0), "fat_tower_middle", rotation, overwrite = true))
                for ((bridgeRotation, bridgeOffset) in FAT_TOWER_BRIDGES) {
                    if (!random.nextBoolean()) continue
                    val bridgeStart =
                        add(
                            pieces,
                            child(
                                last,
                                bridgeOffset,
                                "bridge_end",
                                rotation.rotatedBy(bridgeRotation),
                                overwrite = true,
                            ),
                        )
                    recursiveChildren(Generator.TOWER_BRIDGE, generationDepth + 1, bridgeStart, null, pieces)
                }
                index++
            }
            add(pieces, child(last, StructurePos(-2, 8, -2), "fat_tower_top", rotation, overwrite = true))
            return true
        }

        private fun recursiveChildren(
            generator: Generator,
            generationDepth: Int,
            parent: Piece,
            offset: StructurePos?,
            pieces: MutableList<Piece>,
        ): Boolean {
            if (generationDepth > MAX_GENERATION_DEPTH) return false
            val childPieces = mutableListOf<Piece>()
            if (!generate(generator, generationDepth, parent, offset, childPieces)) return false
            val childTag = random.nextInt()
            for (child in childPieces) {
                child.generationDepth = childTag
                val collision = pieces.firstOrNull { it.boundingBox.intersects(child.boundingBox) }
                if (collision != null && collision.generationDepth != parent.generationDepth) return false
            }
            pieces += childPieces
            return true
        }

        private fun child(
            parent: Piece,
            offset: StructurePos,
            templateName: String,
            rotation: StructureRotation,
            overwrite: Boolean,
        ): Piece {
            val relative = StructureTransform.transform(offset, StructureMirror.NONE, parent.rotation, ZERO)
            return piece(templateName, parent.position + relative, rotation, overwrite)
        }

        private fun piece(
            templateName: String,
            position: StructurePos,
            rotation: StructureRotation,
            overwrite: Boolean,
        ): Piece {
            val template =
                requireNotNull(templateLoader("minecraft:end_city/$templateName")) {
                    "missing End City template minecraft:end_city/$templateName"
                }
            return Piece(templateName, position, rotation, overwrite, template)
        }

        private fun add(
            pieces: MutableList<Piece>,
            piece: Piece,
        ): Piece = piece.also(pieces::add)
    }

    private enum class Generator {
        HOUSE_TOWER,
        TOWER,
        TOWER_BRIDGE,
        FAT_TOWER,
    }

    private fun StructureRotation.rotatedBy(other: StructureRotation): StructureRotation =
        StructureRotation.entries[(ordinal + other.ordinal) and 3]

    private val ZERO = StructurePos(0, 0, 0)
    private const val MAX_GENERATION_DEPTH = 8

    private val TOWER_BRIDGES =
        listOf(
            StructureRotation.NONE to StructurePos(1, -1, 0),
            StructureRotation.CLOCKWISE_90 to StructurePos(6, -1, 1),
            StructureRotation.COUNTERCLOCKWISE_90 to StructurePos(0, -1, 5),
            StructureRotation.CLOCKWISE_180 to StructurePos(5, -1, 6),
        )
    private val FAT_TOWER_BRIDGES =
        listOf(
            StructureRotation.NONE to StructurePos(4, -1, 0),
            StructureRotation.CLOCKWISE_90 to StructurePos(12, -1, 4),
            StructureRotation.COUNTERCLOCKWISE_90 to StructurePos(0, -1, 8),
            StructureRotation.CLOCKWISE_180 to StructurePos(8, -1, 12),
        )
}
