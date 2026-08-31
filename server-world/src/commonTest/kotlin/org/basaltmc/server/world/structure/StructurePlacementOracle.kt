package org.basaltmc.server.world.structure

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the two structure placements. */
class StructurePlacementOracle {
    private fun report(
        name: String,
        grid: StructurePlacementGrid,
        seed: Long,
    ) {
        var placementChunks = 0
        var structureChunks = 0
        var checksum = -3750763034362895579L
        for (chunkX in -48..48) {
            for (chunkZ in -48..48) {
                val (candidateX, candidateZ) = grid.candidateChunk(seed, chunkX, chunkZ)
                checksum = (checksum xor candidateX.toLong()) * 1099511628211L
                checksum = (checksum xor candidateZ.toLong()) * 1099511628211L
                if (grid.isPlacementChunk(seed, chunkX, chunkZ)) placementChunks++
                if (grid.isStructureChunk(seed, chunkX, chunkZ)) {
                    structureChunks++
                    // Which chunks won, not only how many: two salts can agree on the count.
                    checksum = (checksum xor chunkX.toLong()) * 1099511628211L
                    checksum = (checksum xor chunkZ.toLong()) * 1099511628211L
                }
            }
        }
        println("ORACLE $name placement=$placementChunks structure=$structureChunks checksum=$checksum")
    }

    @Test
    fun dumpGrids() {
        val seed = 8_675_309L
        report("village", StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312), seed)
        report(
            "triangular",
            StructurePlacementGrid(
                spacing = 32,
                separation = 8,
                salt = 10387313,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            ),
            seed,
        )
        for (reduction in StructurePlacementGrid.FrequencyReduction.entries) {
            report(
                "freq_${reduction.name}",
                StructurePlacementGrid(
                    spacing = 24,
                    separation = 6,
                    salt = 165745296,
                    frequency = 0.25f,
                    frequencyReduction = reduction,
                ),
                seed,
            )
        }
        val villages = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)
        report(
            "excluded",
            StructurePlacementGrid(
                spacing = 20,
                separation = 5,
                salt = 165745296,
                exclusionZone = StructurePlacementGrid.ExclusionZone(6) { villages },
            ),
            seed,
        )
    }

    @Test
    fun dumpRings() {
        // The biome pattern is deliberately coarse: what matters is that some cells match and some
        // do not, so the reservoir pick is exercised rather than always taking the first.
        val placement =
            ConcentricRingsPlacement(
                distance = 32,
                spread = 3,
                count = 128,
                preferredBiomes = setOf("plains"),
            ) { quartX, _, quartZ -> if ((quartX * 7 + quartZ * 3).mod(5) == 0) "plains" else "ocean" }
        val positions = placement.ringPositions(4242L)
        var checksum = -3750763034362895579L
        for ((x, z) in positions) {
            checksum = (checksum xor x.toLong()) * 1099511628211L
            checksum = (checksum xor z.toLong()) * 1099511628211L
        }
        val first = positions.take(4).joinToString(";") { "${it.first},${it.second}" }
        println("ORACLE rings count=${positions.size} first=$first checksum=$checksum")

        val never =
            ConcentricRingsPlacement(
                distance = 32,
                spread = 3,
                count = 8,
                preferredBiomes = setOf("plains"),
            ) { _, _, _ -> "ocean" }
        val fallback = never.ringPositions(4242L)
        println(
            "ORACLE ringsFallback count=${fallback.size} " +
                "first=${fallback.take(4).joinToString(";") { "${it.first},${it.second}" }}",
        )
        val empty =
            ConcentricRingsPlacement(
                distance = 32,
                spread = 3,
                count = 0,
                preferredBiomes = emptySet(),
            ) { _, _, _ -> "ocean" }
        println("ORACLE ringsEmpty count=${empty.ringPositions(1L).size}")
    }
}
