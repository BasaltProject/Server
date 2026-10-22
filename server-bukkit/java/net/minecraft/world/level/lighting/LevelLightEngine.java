package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
public class LevelLightEngine implements LightEventListener {
    protected LevelLightEngine() {
    }
    public void checkBlock(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.lighting.LevelLightEngine.checkBlock");
    }
    public void queueSectionData(LightLayer layer, SectionPos pos, DataLayer data) {
        throw Unsupported.hit("net.minecraft.world.level.lighting.LevelLightEngine.queueSectionData");
    }
}
