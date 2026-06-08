package org.basaltmc.bukkit.facade;

import net.minecraft.core.Holder;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.chunk.PalettedContainer;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import org.basaltmc.bukkit.WorldNatives;

public final class Chunks {
    private static final int kSection = 4096;

    private Chunks() {}

    public static int[] readAll(String world, int chunkX, int chunkZ, int minSection, int sections) {
        int[] all = new int[sections * kSection];
        int[] one = new int[kSection];
        for (int i = 0; i < sections; i++) {
            if (!WorldNatives.readSection(world, chunkX, minSection + i, chunkZ, false, one)) {
                return null;
            }
            System.arraycopy(one, 0, all, i * kSection, kSection);
        }
        return all;
    }

    public static int[] readBiomes(String world, int chunkX, int chunkZ, int cells) {
        int[] out = new int[cells];
        return WorldNatives.readBiomes(world, chunkX, chunkZ, out) ? out : null;
    }

    public static long hash(int[] ids, int offset) {
        long h = 0xcbf29ce484222325L;
        for (int i = 0; i < kSection; i++) {
            h = (h ^ ids[offset + i]) * 0x100000001b3L;
        }
        return h;
    }

    public static PalettedContainer<BlockState> states(PalettedContainerFactory factory, int[] all, int offset) {
        int[] ids = java.util.Arrays.copyOfRange(all, offset, offset + kSection);
        int limit = Block.BLOCK_STATE_REGISTRY.size();
        for (int i = 0; i < kSection; i++) {
            if (ids[i] < 0 || ids[i] >= limit) {
                ids[i] = 0;
            }
        }
        return PalettedContainer.fromIds(factory.blockStatesStrategy(), ids, Block::stateById, limit);
    }

    public static PalettedContainer<Holder<Biome>> biomes(PalettedContainerFactory factory, int[] cells, int section) {
        if (cells == null) {
            return factory.createForBiomes();
        }
        int[] ids = java.util.Arrays.copyOfRange(cells, section * 64, section * 64 + 64);
        int limit = factory.biomeStrategy().globalMap().size();
        for (int i = 0; i < 64; i++) {
            if (ids[i] < 0 || ids[i] >= limit) {
                ids[i] = 0;
            }
        }
        return PalettedContainer.fromIds(factory.biomeStrategy(), ids,
            id -> factory.biomeStrategy().globalMap().byId(id), limit);
    }

    public static int[] ids(LevelChunkSection section) {
        return section.getStates().toIds(Block::getId);
    }
}
