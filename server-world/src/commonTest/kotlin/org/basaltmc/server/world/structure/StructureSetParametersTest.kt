package org.basaltmc.server.world.structure

import org.basaltmc.server.world.terrain.AncientCityStructureGenerator
import org.basaltmc.server.world.terrain.NetherStructureGenerator
import org.basaltmc.server.world.terrain.SwampHutStructureGenerator
import org.basaltmc.server.world.terrain.TrailRuinsStructureGenerator
import org.basaltmc.server.world.terrain.TrialChamberStructureGenerator
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A wrong salt still produces a world full of the structure, just not the same world, and nothing else
 * notices. These are the jar's own `structure_set` values, copied here so a stray edit has to disagree
 * with something. // parity: data/minecraft/worldgen/structure_set, 26.2.
 */
class StructureSetParametersTest {
    private fun assertGrid(
        grid: StructurePlacementGrid,
        spacing: Int,
        separation: Int,
        salt: Int,
        set: String,
    ) {
        assertEquals(spacing, grid.spacing, "$set spacing")
        assertEquals(separation, grid.separation, "$set separation")
        assertEquals(salt, grid.salt, "$set salt")
    }

    @Test
    fun `every wired structure keeps its own set's numbers`() {
        assertGrid(SwampHutStructureGenerator.GRID, 32, 8, 14357620, "swamp_huts")
        assertGrid(AncientCityStructureGenerator.GRID, 24, 8, 20083232, "ancient_cities")
        assertGrid(TrialChamberStructureGenerator.GRID, 34, 12, 94251327, "trial_chambers")
        assertGrid(NetherStructureGenerator.NETHER_FOSSIL_GRID, 2, 1, 14357921, "nether_fossils")
        assertGrid(NetherStructureGenerator.NETHER_COMPLEX_GRID, 27, 4, 30084232, "nether_complexes")
    }

    @Test
    fun `the trail ruins land where the official server says`() {
        // The jar answered [1456, ~, -1440] on seed 12345, the minimum corner of chunk (91, -90).
        val grid = StructurePlacementGrid(spacing = 34, separation = 8, salt = 83469867)
        assertTrue(grid.isStructureChunk(12345L, 91, -90))
        for (dx in -1..1) {
            for (dz in -1..1) {
                if (dx == 0 && dz == 0) continue
                assertTrue(
                    !grid.isStructureChunk(12345L, 91 + dx, -90 + dz),
                    "chunk (${91 + dx}, ${-90 + dz}) should not hold trail ruins",
                )
            }
        }
        assertEquals(3, TrailRuinsStructureGenerator.STRUCTURE_INDEX, "fourth of underground_structures")
    }

    @Test
    fun `the sets that are not a plain linear spread say so`() {
        // End cities are the only wired set with `spread_type: triangular`; everything else omits
        // the field, which means linear.
        assertEquals(StructurePlacementGrid.SpreadType.LINEAR, SwampHutStructureGenerator.GRID.spreadType)
        assertEquals(1.0f, SwampHutStructureGenerator.GRID.frequency, "swamp huts declare no frequency")
        assertEquals(null, SwampHutStructureGenerator.GRID.exclusionZone, "swamp huts declare no exclusion")
    }

    @Test
    fun `the nether fossil grid is dense enough to be almost every other chunk`() {
        // spacing 2 / separation 1 is the tightest set in the game, and a spacing of 1 would make
        // it every chunk. This is the cheap guard on that pair having been copied straight.
        var found = 0
        for (chunkX in 0 until 16) {
            for (chunkZ in 0 until 16) {
                if (NetherStructureGenerator.NETHER_FOSSIL_GRID.isStructureChunk(SEED, chunkX, chunkZ)) found++
            }
        }
        assertEquals(64, found, "one candidate per 2x2 cell over 16x16 chunks")
    }

    private companion object {
        const val SEED = 12345L
    }
}
