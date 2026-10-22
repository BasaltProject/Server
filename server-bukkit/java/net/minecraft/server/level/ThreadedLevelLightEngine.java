package net.minecraft.server.level;

import java.util.Collection;
import java.util.function.Consumer;
import java.util.function.IntConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.lighting.LayerLightEventListener;
import net.minecraft.world.level.lighting.LevelLightEngine;
import org.basaltmc.bukkit.Unsupported;
public class ThreadedLevelLightEngine extends LevelLightEngine {
    ThreadedLevelLightEngine() {
    }
    public void checkBlock(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.server.level.ThreadedLevelLightEngine.checkBlock");
    }
    public LayerLightEventListener getLayerListener(LightLayer lightLayer) {
        return new LayerLightEventListener() {
            @Override
            public net.minecraft.world.level.chunk.DataLayer getDataLayerData(net.minecraft.core.SectionPos pos) {
                return null;
            }
        };
    }
    public int starlight$serverRelightChunks(Collection collection, Consumer consumer, IntConsumer intConsumer) {
        throw Unsupported.hit("net.minecraft.server.level.ThreadedLevelLightEngine.starlight$serverRelightChunks");
    }
}
