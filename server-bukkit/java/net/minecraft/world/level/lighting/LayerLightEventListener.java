package net.minecraft.world.level.lighting;

import net.minecraft.core.SectionPos;
import net.minecraft.world.level.chunk.DataLayer;
public interface LayerLightEventListener extends LightEventListener {
    public default DataLayer getDataLayerData(SectionPos pos) {
        return null;
    }
}
