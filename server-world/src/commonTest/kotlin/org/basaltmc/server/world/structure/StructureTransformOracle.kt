package org.basaltmc.server.world.structure

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the structure transform, directions and position queries. */
class StructureTransformOracle {
    @Test
    fun dumpTransform() {
        for (mirror in StructureMirror.entries) {
            for (rotation in StructureRotation.entries) {
                var checksum = -3750763034362895579L
                for (index in 0 until 512) {
                    val pos = StructurePos((index * 7).mod(23) - 11, (index * 3).mod(17) - 8, (index * 11).mod(29) - 14)
                    val pivot = StructurePos((index * 5).mod(9) - 4, 0, (index * 13).mod(9) - 4)
                    val moved = StructureTransform.transform(pos, mirror, rotation, pivot)
                    checksum = (checksum xor moved.x.toLong()) * 1099511628211L
                    checksum = (checksum xor moved.y.toLong()) * 1099511628211L
                    checksum = (checksum xor moved.z.toLong()) * 1099511628211L
                }
                println("ORACLE ${mirror.name}_${rotation.name} checksum=$checksum")
            }
        }
    }

    @Test
    fun dumpDirections() {
        val rows = StringBuilder()
        for (direction in StructureDirection.entries) {
            rows.append(direction.name).append(':')
            rows
                .append(direction.dx)
                .append(',')
                .append(direction.dy)
                .append(',')
                .append(direction.dz)
            rows.append('/').append(direction.isHorizontal)
            rows.append('/').append(direction.opposite().name)
            for (mirror in StructureMirror.entries) rows.append('/').append(direction.mirror(mirror).name)
            for (rotation in StructureRotation.entries) rows.append('/').append(direction.rotate(rotation).name)
            rows.append(';')
        }
        println("ORACLE directions rows=$rows")
        println(
            "ORACLE names rotation=${StructureRotation.entries.joinToString(",") { it.serializedName }} " +
                "mirror=${StructureMirror.entries.joinToString(",") { it.serializedName }} " +
                "fromName=${StructureDirection.fromName("NoRtH")?.name}/${StructureDirection.fromName("nope")?.name}",
        )
    }

    @Test
    fun dumpQueries() {
        fun box(
            minX: Int,
            minY: Int,
            minZ: Int,
            maxX: Int,
            maxY: Int,
            maxZ: Int,
        ) = StructureBoundingBox(minX, minY, minZ, maxX, maxY, maxZ)

        val fortress =
            StructureStartRecord.of("fortress", 0, 0, listOf(box(0, 60, 0, 8, 70, 8), box(20, 60, 0, 28, 70, 8)))!!
        val city = StructureStartRecord.of("ancient_city", 0, 0, listOf(box(4, 55, 4, 12, 65, 12)))!!
        val chunk = ChunkStructures()
        chunk.add(fortress)
        chunk.add(city)
        // The same start offered twice is dropped rather than duplicated.
        chunk.add(fortress)
        val queries = StructureQueries { x, z -> if (x == 0 && z == 0) chunk else null }
        val probes =
            listOf(
                StructurePos(5, 62, 5),
                StructurePos(15, 62, 5),
                StructurePos(24, 62, 5),
                StructurePos(5, 58, 5),
                StructurePos(100, 62, 100),
            )
        val rows = StringBuilder()
        for (pos in probes) {
            rows
                .append(pos.x)
                .append(',')
                .append(pos.y)
                .append(',')
                .append(pos.z)
                .append(':')
            rows.append(queries.startsIn(pos).size)
            rows.append('/').append(queries.startAt(pos, "fortress")?.structure ?: "-")
            rows.append('/').append(queries.startWithPieceAt(pos, "fortress")?.structure ?: "-")
            rows.append('/').append(queries.startAt(pos, "ancient_city")?.structure ?: "-")
            rows.append('/').append(queries.startWithPieceAt(pos, "ancient_city")?.structure ?: "-")
            rows.append(';')
        }
        println(
            "ORACLE queries rows=$rows starts=${chunk.starts().size} has=${chunk.has("fortress")}/" +
                "${chunk.has("igloo")} empty=${chunk.isEmpty} " +
                "fullBounds=${fortress.fullBounds.minX},${fortress.fullBounds.maxX}",
        )
    }
}
