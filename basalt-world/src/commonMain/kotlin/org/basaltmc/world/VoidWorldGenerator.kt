package org.basaltmc.world

import org.basaltmc.registry.Blocks

/**
 * Nothing at all: every chunk is air.
 *
 * There is no vanilla level type that is exactly this (its closest, the "The Void" superflat
 * preset, is a flat world with an empty layer stack plus a small platform at spawn), so this is a
 * Basalt type rather than a parity one. It exists because a lobby or a minigame hub wants an empty
 * canvas and no terrain generation cost at all, which today means installing a plugin.
 *
 * [platformRadius] blocks of stone are placed under the world spawn so that whoever joins a fresh
 * void world does not fall out of it before they can build anything. Set it to 0 for a truly empty
 * world, which is what you want once the lobby is built and pasted in.
 */
public object VoidWorldGenerator {
    public const val DEFAULT_PLATFORM_RADIUS: Int = 4

    public fun generate(
        chunkX: Int,
        chunkZ: Int,
        spawnX: Int = 8,
        spawnY: Int = 64,
        spawnZ: Int = 8,
        platformRadius: Int = DEFAULT_PLATFORM_RADIUS,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        if (platformRadius <= 0) return chunk
        // Only the chunks the platform actually reaches do any work; everything else stays air.
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val worldX = chunkX * 16 + x
                val worldZ = chunkZ * 16 + z
                if (worldX < spawnX - platformRadius || worldX > spawnX + platformRadius) continue
                if (worldZ < spawnZ - platformRadius || worldZ > spawnZ + platformRadius) continue
                chunk.setBlockStateId(x, spawnY - 1, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }
}
