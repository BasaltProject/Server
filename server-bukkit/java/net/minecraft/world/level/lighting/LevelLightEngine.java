// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.LightChunkGetter;
public class LevelLightEngine implements LightEventListener {
    protected LevelLightEngine() {
        this.levelHeightAccessor = null;
    }
    public void checkBlock(BlockPos pos) {
        return;
    }
    public void queueSectionData(LightLayer layer, SectionPos pos, DataLayer data) {
        return;
    }
    public static final int LIGHT_SECTION_PADDING = 1;
    public static final LevelLightEngine EMPTY = new LevelLightEngine();
    protected final LevelHeightAccessor levelHeightAccessor;
    public LevelLightEngine(LightChunkGetter chunkSource, boolean hasBlockLight, boolean hasSkyLight) {
        this.levelHeightAccessor = chunkSource == null ? null : chunkSource.getLevel();
    }
    public boolean hasLightWork() {
        return false;
    }
    public int runLightUpdates() {
        return 0;
    }
    public void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {
        return;
    }
    public void setLightEnabled(ChunkPos pos, boolean enable) {
        return;
    }
    public void propagateLightSources(ChunkPos pos) {
        return;
    }
    public LayerLightEventListener getLayerListener(LightLayer layer) {
        return LayerLightEventListener.DummyLightLayerEventListener.INSTANCE;
    }
    public String getDebugData(LightLayer layer, SectionPos pos) {
        return "n/a";
    }
    public LayerLightSectionStorage.SectionType getDebugSectionType(LightLayer layer, SectionPos pos) {
        return null;
    }
    public void retainData(ChunkPos pos, boolean retain) {
        return;
    }
    public int getRawBrightness(BlockPos pos, int skyDampen) {
        return 0;
    }
    public boolean lightOnInColumn(long sectionZeroNode) {
        return true;
    }
    public int getLightSectionCount() {
        return getMaxLightSection() - getMinLightSection() + 1;
    }
    public int getMinLightSection() {
        return levelHeightAccessor == null ? -5 : levelHeightAccessor.getMinSectionY() - 1;
    }
    public int getMaxLightSection() {
        return levelHeightAccessor == null ? 20 : levelHeightAccessor.getMaxSectionY() + 1;
    }
}
