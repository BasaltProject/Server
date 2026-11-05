// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.core.SectionPos;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;
public interface LayerLightEventListener extends LightEventListener {
    public default DataLayer getDataLayerData(SectionPos pos) {
        return null;
    }
    default int getLightValue(BlockPos pos) {
        return 0;
    }
    public static enum DummyLightLayerEventListener implements LayerLightEventListener {
        INSTANCE;
        public DataLayer getDataLayerData(SectionPos pos) {
            return null;
        }
        public int getLightValue(BlockPos pos) {
            return 0;
        }
        public void checkBlock(BlockPos pos) {

        }
        public boolean hasLightWork() {
            return false;
        }
        public int runLightUpdates() {
            return 0;
        }
        public void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {

        }
        public void setLightEnabled(ChunkPos pos, boolean enable) {

        }
        public void propagateLightSources(ChunkPos pos) {

        }
    }
}
