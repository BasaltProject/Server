// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.lighting.LevelLightEngine;
public interface BlockAndLightGetter extends BlockGetter {
    LevelLightEngine getLightEngine();
    default int getBrightness(LightLayer layer, BlockPos pos) {
        return getLightEngine().getLayerListener(layer).getLightValue(pos);
    }
    default int getRawBrightness(BlockPos pos, int darkening) {
        return getLightEngine().getRawBrightness(pos, darkening);
    }
    default boolean canSeeSky(BlockPos pos) {
        return getBrightness(LightLayer.SKY, pos) >= 15;
    }
}
