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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener#getLightValue");
    }
    public static enum DummyLightLayerEventListener implements LayerLightEventListener {
        INSTANCE;
        public DataLayer getDataLayerData(SectionPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#getDataLayerData");
        }
        public int getLightValue(BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#getLightValue");
        }
        public void checkBlock(BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#checkBlock");
        }
        public boolean hasLightWork() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#hasLightWork");
        }
        public int runLightUpdates() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#runLightUpdates");
        }
        public void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#updateSectionStatus");
        }
        public void setLightEnabled(ChunkPos pos, boolean enable) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#setLightEnabled");
        }
        public void propagateLightSources(ChunkPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.lighting.LayerLightEventListener.DummyLightLayerEventListener#propagateLightSources");
        }
    }
}
