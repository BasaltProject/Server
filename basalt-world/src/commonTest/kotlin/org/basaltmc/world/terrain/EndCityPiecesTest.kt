package org.basaltmc.world.terrain

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureRotation
import org.basaltmc.world.structure.StructureTemplate
import kotlin.test.Test
import kotlin.test.assertEquals

class EndCityPiecesTest {
    @Test
    fun matchesOfficialRecursiveAssemblyVectors() {
        for (vector in VECTORS) {
            val random = WorldgenRandom(LegacyRandom(0L))
            random.setLargeFeatureSeed(vector.seed, 3, 4)
            val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
            val pieces =
                EndCityPieces.assemble(
                    TEMPLATE_LOADER,
                    StructurePos(55, 70, 71),
                    rotation,
                    random,
                )

            assertEquals(vector.rotation, rotation, "rotation for seed ${vector.seed}")
            assertEquals(vector.count, pieces.size, "piece count for seed ${vector.seed}")
            assertEquals(vector.ships, pieces.count { it.templateName == "ship" }, "ship count for seed ${vector.seed}")
            assertEquals(vector.checksum, pieces.checksum(), "piece checksum for seed ${vector.seed}")
        }
    }

    private fun List<EndCityPieces.Piece>.checksum(): Long {
        var result = 1L
        for (piece in this) {
            result = result * 31 + piece.templateName.javaHashCode()
            result = result * 31 + piece.position.x
            result = result * 31 + piece.position.y
            result = result * 31 + piece.position.z
            result = result * 31 + piece.rotation.ordinal
            result = result * 31 + piece.generationDepth
            result = result * 31 + piece.boundingBox.minX
            result = result * 31 + piece.boundingBox.minY
            result = result * 31 + piece.boundingBox.minZ
            result = result * 31 + piece.boundingBox.maxX
            result = result * 31 + piece.boundingBox.maxY
            result = result * 31 + piece.boundingBox.maxZ
        }
        return result
    }

    private fun String.javaHashCode(): Int {
        var result = 0
        for (character in this) result = 31 * result + character.code
        return result
    }

    private data class Vector(
        val seed: Long,
        val rotation: StructureRotation,
        val count: Int,
        val ships: Int,
        val checksum: Long,
    )

    private companion object {
        val VECTORS =
            listOf(
                Vector(0L, StructureRotation.NONE, 72, 1, 8831025712701590139L),
                Vector(1L, StructureRotation.NONE, 27, 1, -8232730242998506868L),
                Vector(2L, StructureRotation.CLOCKWISE_180, 52, 1, 1071340219773786246L),
                Vector(3L, StructureRotation.COUNTERCLOCKWISE_90, 88, 1, -3860240471414235491L),
                Vector(4L, StructureRotation.NONE, 21, 1, 5310972949568502800L),
                Vector(42L, StructureRotation.CLOCKWISE_180, 63, 0, 4948665190051769392L),
                Vector(-1L, StructureRotation.CLOCKWISE_90, 122, 1, -6926887142051444292L),
                Vector(123456789L, StructureRotation.CLOCKWISE_90, 39, 0, 4785521116868288616L),
            )

        val TEMPLATE_SIZES =
            mapOf(
                "base_floor" to StructurePos(10, 4, 10),
                "base_roof" to StructurePos(12, 2, 12),
                "second_floor_1" to StructurePos(12, 8, 12),
                "second_floor_2" to StructurePos(12, 8, 12),
                "second_roof" to StructurePos(14, 2, 14),
                "third_floor_1" to StructurePos(14, 8, 14),
                "third_floor_2" to StructurePos(14, 8, 14),
                "third_roof" to StructurePos(16, 2, 16),
                "tower_base" to StructurePos(7, 7, 7),
                "tower_piece" to StructurePos(7, 4, 7),
                "tower_top" to StructurePos(9, 5, 9),
                "bridge_end" to StructurePos(5, 6, 2),
                "bridge_piece" to StructurePos(5, 6, 4),
                "bridge_steep_stairs" to StructurePos(5, 7, 4),
                "bridge_gentle_stairs" to StructurePos(5, 7, 8),
                "fat_tower_base" to StructurePos(13, 4, 13),
                "fat_tower_middle" to StructurePos(13, 8, 13),
                "fat_tower_top" to StructurePos(17, 6, 17),
                "ship" to StructurePos(13, 24, 29),
            )

        val TEMPLATE_LOADER: (String) -> StructureTemplate? = { location ->
            TEMPLATE_SIZES[location.substringAfterLast('/')]?.let(::emptyTemplate)
        }

        fun emptyTemplate(size: StructurePos): StructureTemplate {
            val root = NbtCompound()
            root["size"] = NbtList(NbtType.INT, listOf(NbtInt(size.x), NbtInt(size.y), NbtInt(size.z)))
            root["palette"] = NbtList(NbtType.COMPOUND, emptyList())
            root["blocks"] = NbtList(NbtType.COMPOUND, emptyList())
            return StructureTemplate().apply { load(root) }
        }
    }
}
