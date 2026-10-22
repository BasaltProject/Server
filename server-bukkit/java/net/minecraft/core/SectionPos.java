package net.minecraft.core;

import net.minecraft.world.level.ChunkPos;

public class SectionPos extends Vec3i {
    protected SectionPos(int x, int y, int z) {
        super(x, y, z);
    }

    public static SectionPos of(int x, int y, int z) {
        return new SectionPos(x, y, z);
    }

    public static SectionPos of(ChunkPos pos, int sectionY) {
        return new SectionPos(pos.x(), sectionY, pos.z());
    }

    public static int blockToSectionCoord(int blockCoord) {
        return blockCoord >> 4;
    }

    public static int sectionToBlockCoord(int sectionCoord) {
        return sectionCoord << 4;
    }
}
