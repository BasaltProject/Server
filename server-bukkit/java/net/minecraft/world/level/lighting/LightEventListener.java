// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
public interface LightEventListener {
    default void checkBlock(BlockPos pos) {

    }
    default boolean hasLightWork() {
        return false;
    }
    default int runLightUpdates() {
        return 0;
    }
    default void updateSectionStatus(BlockPos pos, boolean sectionEmpty) {
        updateSectionStatus(SectionPos.of(pos), sectionEmpty);
    }
    default void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {

    }
    default void setLightEnabled(ChunkPos pos, boolean enable) {

    }
    default void propagateLightSources(ChunkPos pos) {

    }
}
