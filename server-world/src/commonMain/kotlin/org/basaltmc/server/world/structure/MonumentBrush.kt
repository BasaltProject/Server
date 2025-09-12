package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/** // parity: the palette every monument piece shares. */
internal object MonumentPalette {
    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    val BASE_GRAY: Int = state("prismarine")
    val BASE_LIGHT: Int = state("prismarine_bricks")
    val BASE_BLACK: Int = state("dark_prismarine")
    val DOT: Int = BASE_LIGHT
    val LAMP: Int = state("sea_lantern")
    val WATER: Int = state("water")
    val GOLD: Int = state("gold_block")
    val WET_SPONGE: Int = state("wet_sponge")

    /** // parity: FILL_KEEP, the blocks a water box refuses to touch. */
    val FILL_KEEP: Set<String> = setOf("ice", "packed_ice", "blue_ice", "water")

    /** // parity: StructurePiece.isReplaceableByStructures. */
    val REPLACEABLE: Set<String> = setOf("water", "lava", "glow_lichen", "seagrass", "tall_seagrass")

    const val BOTTOM_Y: Int = -64
}

/**
 * // parity: the drawing half of OceanMonumentPiece. Every monument piece writes through these, so
 * the flooding rule and the "only where the water already is" rule are stated once.
 */
internal class MonumentBrush(
    val fill: StructurePieceFill,
    private val seaLevel: Int,
    private val clip: StructureBoundingBox,
) {
    fun place(
        stateId: Int,
        x: Int,
        y: Int,
        z: Int,
    ) {
        fill.place(stateId, x, y, z)
    }

    fun blockAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = fill.blockAt(x, y, z)

    fun box(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        stateId: Int,
    ) {
        fill.generateBox(x0, y0, z0, x1, y1, z1, stateId, stateId, skipAir = false)
    }

    /**
     * // parity: generateWaterBox. Ice of any kind and water already there are left alone; above sea
     * level the box is air instead, which is what opens the monument's roof to the sky.
     */
    fun waterBox(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    val existing = fill.blockAt(x, y, z)
                    if (path(existing) in MonumentPalette.FILL_KEEP) continue
                    if (fill.worldY(y) >= seaLevel && existing != MonumentPalette.WATER) {
                        fill.place(Blocks.AIR.stateId, x, y, z)
                    } else {
                        fill.place(MonumentPalette.WATER, x, y, z)
                    }
                }
            }
        }
    }

    /** // parity: generateBoxOnFillOnly, which seals a ceiling only where the room is still flooded. */
    fun boxOnFillOnly(
        x0: Int,
        y0: Int,
        z0: Int,
        x1: Int,
        y1: Int,
        z1: Int,
        stateId: Int,
    ) {
        for (y in y0..y1) {
            for (x in x0..x1) {
                for (z in z0..z1) {
                    if (fill.blockAt(x, y, z) == MonumentPalette.WATER) fill.place(stateId, x, y, z)
                }
            }
        }
    }

    /** // parity: generateDefaultFloor. A room above the ground floor lays its own, holed if it opens downward. */
    fun defaultFloor(
        xOff: Int,
        zOff: Int,
        downOpening: Boolean,
    ) {
        val gray = MonumentPalette.BASE_GRAY
        val light = MonumentPalette.BASE_LIGHT
        if (!downOpening) {
            box(xOff, 0, zOff, xOff + 7, 0, zOff + 7, gray)
            return
        }
        box(xOff, 0, zOff, xOff + 2, 0, zOff + 7, gray)
        box(xOff + 5, 0, zOff, xOff + 7, 0, zOff + 7, gray)
        box(xOff + 3, 0, zOff, xOff + 4, 0, zOff + 2, gray)
        box(xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 7, gray)
        box(xOff + 3, 0, zOff + 2, xOff + 4, 0, zOff + 2, light)
        box(xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 5, light)
        box(xOff + 2, 0, zOff + 3, xOff + 2, 0, zOff + 4, light)
        box(xOff + 5, 0, zOff + 3, xOff + 5, 0, zOff + 4, light)
    }

    /** // parity: StructurePiece.fillColumnDown, which stops at the first block it may not replace. */
    fun fillColumnDown(
        stateId: Int,
        x: Int,
        startY: Int,
        z: Int,
    ) {
        var y = startY
        while (fill.worldY(y) > MonumentPalette.BOTTOM_Y + 1) {
            val existing = fill.blockAt(x, y, z)
            if (!Blocks.isAir(existing) && path(existing) !in MonumentPalette.REPLACEABLE) break
            fill.place(stateId, x, y, z)
            y--
        }
    }

    /** // parity: spawnElder, which stands a healed elder guardian on the block rather than inside it. */
    fun spawnElder(
        x: Int,
        y: Int,
        z: Int,
    ) {
        fill.placeEntity(Identifier.vanilla("elder_guardian"), x, y, z, NbtCompound(emptyMap()), yOffset = 0.0)
    }

    /** // parity: chunkIntersects, the per-slice guard that keeps a piece from drawing 58 blocks wide every chunk. */
    fun chunkIntersects(
        x0: Int,
        z0: Int,
        x1: Int,
        z1: Int,
    ): Boolean {
        val wx0 = fill.worldX(x0, z0)
        val wz0 = fill.worldZ(x0, z0)
        val wx1 = fill.worldX(x1, z1)
        val wz1 = fill.worldZ(x1, z1)
        return clip.maxX >= minOf(wx0, wx1) &&
            clip.minX <= maxOf(wx0, wx1) &&
            clip.maxZ >= minOf(wz0, wz1) &&
            clip.minZ <= maxOf(wz0, wz1)
    }

    private fun path(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path
}
