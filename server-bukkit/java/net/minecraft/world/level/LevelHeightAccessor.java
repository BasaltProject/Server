// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
public interface LevelHeightAccessor {
    default int getHeight() {
        return getMaxY() - getMinY() + 1;
    }
    default int getMinY() {
        return 0;
    }
    default int getMaxY() {
        return getMinY() + getHeight() - 1;
    }
    default int getSectionsCount() {
        return getMaxSectionY() - getMinSectionY() + 1;
    }
    default int getMinSectionY() {
        return getMinY() >> 4;
    }
    default int getMaxSectionY() {
        return getMaxY() >> 4;
    }
    default boolean isInsideBuildHeight(BlockPos pos) {
        return isInsideBuildHeight(pos.getY());
    }
    default boolean isInsideBuildHeight(int blockY) {
        return blockY >= getMinY() && blockY <= getMaxY();
    }
    default boolean isOutsideBuildHeight(BlockPos pos) {
        return isOutsideBuildHeight(pos.getY());
    }
    default boolean isOutsideBuildHeight(int blockY) {
        return !isInsideBuildHeight(blockY);
    }
    default int getSectionIndex(int blockY) {
        return getSectionIndexFromSectionY(blockY >> 4);
    }
    default int getSectionIndexFromSectionY(int sectionY) {
        return sectionY - getMinSectionY();
    }
    default int getSectionYFromSectionIndex(int sectionIndex) {
        return sectionIndex + getMinSectionY();
    }
    static LevelHeightAccessor create(int minY, int height) {
        return new LevelHeightAccessor() {
            @Override
            public int getHeight() {
                return height;
            }

            @Override
            public int getMinY() {
                return minY;
            }
        };
    }
}
