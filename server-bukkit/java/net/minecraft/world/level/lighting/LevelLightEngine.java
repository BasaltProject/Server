package net.minecraft.world.level.lighting;

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
public class LevelLightEngine implements LightEventListener {
    protected LevelLightEngine() {
    }
    public void checkBlock(BlockPos pos) {
        return;
    }
    public void queueSectionData(LightLayer layer, SectionPos pos, DataLayer data) {
        return;
    }
}
