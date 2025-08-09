package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

/** Shared pumpkin, sugar-cane, cactus and 26.2 firefly-bush placed features. */
public object ExtraVegetationDecoration {
    public val PUMPKIN_ONLY: BiomeVegetalDecoration = BiomeVegetalDecoration(::placePumpkins)

    public val DEFAULT: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placePumpkins(chunk, random)
            placeSugarCane(chunk, random, rarity = 6)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    /** Default sugar-cane/firefly tail after pumpkin moved to its independent feature seed. */
    public val DEFAULT_WITHOUT_PUMPKIN: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 6)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    /** Shared near-water firefly tail after pumpkin and sugar cane got independent seeds. */
    public val FIREFLY_NEAR_WATER: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    public val NEAR_WATER: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 6)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    public val DESERT: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = null)
            placePumpkins(chunk, random)
            placeCactus(chunk, random, rarity = 6)
        }

    public val DESERT_WITHOUT_PUMPKIN: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = null)
            placeCactus(chunk, random, rarity = 6)
        }

    /** Desert sugar-cane tail after pumpkin and cactus moved to independent feature seeds. */
    public val DESERT_WITHOUT_PUMPKIN_OR_CACTUS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = null)
        }

    public val BADLANDS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 5)
            placePumpkins(chunk, random)
            placeCactus(chunk, random, rarity = 13)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    public val BADLANDS_WITHOUT_PUMPKIN: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 5)
            placeCactus(chunk, random, rarity = 13)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    /** Badlands sugar-cane/firefly tail after pumpkin and cactus got independent feature seeds. */
    public val BADLANDS_WITHOUT_PUMPKIN_OR_CACTUS: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 5)
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    public val SWAMP: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 3)
            placePumpkins(chunk, random)
            placeGeneralFireflies(chunk, random, rarity = 8)
            placeNearWaterFireflies(chunk, random, outerCount = 3, noLeavesHeightmap = false)
        }

    public val SWAMP_WITHOUT_PUMPKIN: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeSugarCane(chunk, random, rarity = 3)
            placeGeneralFireflies(chunk, random, rarity = 8)
            placeNearWaterFireflies(chunk, random, outerCount = 3, noLeavesHeightmap = false)
        }

    /** Both swamp firefly features after pumpkin and sugar cane moved to independent seeds. */
    public val SWAMP_FIREFLIES: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeGeneralFireflies(chunk, random, rarity = 8)
            placeNearWaterFireflies(chunk, random, outerCount = 3, noLeavesHeightmap = false)
        }

    public val MANGROVE: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            placeNearWaterFireflies(chunk, random, outerCount = 2, noLeavesHeightmap = true)
        }

    private fun placePumpkins(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        if (random.nextFloat() >= 1.0f / 300.0f) return
        val origin = inSquareAtHeight(chunk, random, Chunk.HeightmapType.MOTION_BLOCKING)
        repeat(96) {
            val pos = triangleOffset(origin, random, 7, 3)
            if (isAir(chunk, pos) && get(chunk, pos.x, pos.y - 1, pos.z) == Blocks.GRASS_BLOCK.stateId) {
                set(chunk, pos, Blocks.PUMPKIN.stateId)
            }
        }
    }

    private fun placeSugarCane(
        chunk: Chunk,
        random: WorldgenRandom,
        rarity: Int?,
    ) {
        if (rarity != null && random.nextFloat() >= 1.0f / rarity) return
        val origin = inSquareAtHeight(chunk, random, Chunk.HeightmapType.MOTION_BLOCKING)
        repeat(20) {
            val pos = triangleOffset(origin, random, 4, 0)
            if (!isAir(chunk, pos) || !canSugarCaneSurvive(chunk, pos)) return@repeat
            val height = 2 + random.nextInt(random.nextInt(3) + 1)
            var actualHeight = height
            for (dy in 1..height) {
                if (!isAir(chunk, pos.copy(y = pos.y + dy))) {
                    actualHeight = dy - 1
                    break
                }
            }
            for (dy in 0 until actualHeight) set(chunk, pos.copy(y = pos.y + dy), Blocks.SUGAR_CANE.stateId)
        }
    }

    private fun placeNearWaterFireflies(
        chunk: Chunk,
        random: WorldgenRandom,
        outerCount: Int,
        noLeavesHeightmap: Boolean,
    ) {
        val type =
            if (noLeavesHeightmap) {
                Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES
            } else {
                Chunk.HeightmapType.MOTION_BLOCKING
            }
        repeat(outerCount) {
            val origin = inSquareAtHeight(chunk, random, type)
            if (!isAir(chunk, origin) ||
                !canBushSurvive(chunk, origin) ||
                !hasAdjacentWaterBelow(chunk, origin)
            ) {
                return@repeat
            }
            repeat(20) {
                val pos = triangleOffset(origin, random, 4, 3)
                if (isAir(chunk, pos) && canBushSurvive(chunk, pos)) set(chunk, pos, Blocks.FIREFLY_BUSH.stateId)
            }
        }
    }

    private fun placeGeneralFireflies(
        chunk: Chunk,
        random: WorldgenRandom,
        rarity: Int,
    ) {
        if (random.nextFloat() >= 1.0f / rarity) return
        val origin = inSquareAtHeight(chunk, random, Chunk.HeightmapType.MOTION_BLOCKING)
        repeat(20) {
            val pos = triangleOffset(origin, random, 4, 3)
            if (isAir(chunk, pos) && canBushSurvive(chunk, pos)) set(chunk, pos, Blocks.FIREFLY_BUSH.stateId)
        }
    }

    private fun placeCactus(
        chunk: Chunk,
        random: WorldgenRandom,
        rarity: Int,
    ) {
        if (random.nextFloat() >= 1.0f / rarity) return
        val origin = inSquareAtHeight(chunk, random, Chunk.HeightmapType.MOTION_BLOCKING)
        repeat(10) {
            val pos = triangleOffset(origin, random, 7, 3)
            if (isAir(chunk, pos) && canCactusSurvive(chunk, pos)) {
                CactusFeature.place(
                    chunk,
                    random,
                    pos.copy(x = chunk.chunkX * 16 + pos.x, z = chunk.chunkZ * 16 + pos.z),
                )
            }
        }
    }

    private fun inSquareAtHeight(
        chunk: Chunk,
        random: WorldgenRandom,
        type: Chunk.HeightmapType,
    ): DecorationPos {
        val x = random.nextInt(16)
        val z = random.nextInt(16)
        return DecorationPos(x, chunk.heightAt(x, z, type), z)
    }

    private fun triangleOffset(
        origin: DecorationPos,
        random: WorldgenRandom,
        horizontal: Int,
        vertical: Int,
    ): DecorationPos =
        DecorationPos(
            origin.x + random.nextInt(horizontal + 1) - random.nextInt(horizontal + 1),
            origin.y + if (vertical == 0) 0 else random.nextInt(vertical + 1) - random.nextInt(vertical + 1),
            origin.z + random.nextInt(horizontal + 1) - random.nextInt(horizontal + 1),
        )

    private fun canSugarCaneSurvive(
        chunk: Chunk,
        pos: DecorationPos,
    ): Boolean {
        val below = get(chunk, pos.x, pos.y - 1, pos.z)
        return (below == Blocks.SUGAR_CANE.stateId || below in sugarCaneSupportStateIds) &&
            hasAdjacentWaterBelow(chunk, pos)
    }

    private fun canBushSurvive(
        chunk: Chunk,
        pos: DecorationPos,
    ): Boolean = get(chunk, pos.x, pos.y - 1, pos.z) in bushSupportStateIds

    private fun canCactusSurvive(
        chunk: Chunk,
        pos: DecorationPos,
    ): Boolean {
        if (get(chunk, pos.x, pos.y - 1, pos.z) !in cactusSupportStateIds) return false
        return listOf(pos.x + 1 to pos.z, pos.x - 1 to pos.z, pos.x to pos.z + 1, pos.x to pos.z - 1).all { (x, z) ->
            val stateId = get(chunk, x, pos.y, z)
            Blocks.isAir(stateId) || !Chunk.HeightmapType.OCEAN_FLOOR.matches(stateId)
        }
    }

    private fun hasAdjacentWaterBelow(
        chunk: Chunk,
        pos: DecorationPos,
    ): Boolean =
        supportsSugarCaneFromSide(get(chunk, pos.x + 1, pos.y - 1, pos.z)) ||
            supportsSugarCaneFromSide(get(chunk, pos.x - 1, pos.y - 1, pos.z)) ||
            supportsSugarCaneFromSide(get(chunk, pos.x, pos.y - 1, pos.z + 1)) ||
            supportsSugarCaneFromSide(get(chunk, pos.x, pos.y - 1, pos.z - 1))

    private fun supportsSugarCaneFromSide(stateId: Int): Boolean =
        Blocks.hasWaterFluid(stateId) || stateId in frostedIceStateIds

    private fun isAir(
        chunk: Chunk,
        pos: DecorationPos,
    ): Boolean = Blocks.isAir(get(chunk, pos.x, pos.y, pos.z))

    private fun get(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int =
        if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.getBlockStateId(x and 15, y, z and 15)
        } else {
            Blocks.AIR.stateId
        }

    private fun set(
        chunk: Chunk,
        pos: DecorationPos,
        stateId: Int,
    ) {
        if (pos.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.setBlockStateId(pos.x and 15, pos.y, pos.z and 15, stateId)
        }
    }

    private fun stateIds(vararg names: String): Set<Int> =
        names.mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()

    private val substrateOverworldStateIds =
        stateIds(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
        )
    private val bushSupportStateIds = substrateOverworldStateIds + stateIds("farmland")
    private val sugarCaneSupportStateIds = substrateOverworldStateIds + stateIds("sand", "red_sand", "suspicious_sand")
    private val frostedIceStateIds = stateIds("frosted_ice")
    private val cactusSupportStateIds = stateIds("cactus", "sand", "red_sand", "suspicious_sand")
}
