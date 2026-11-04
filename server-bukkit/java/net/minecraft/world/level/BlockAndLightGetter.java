// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.lighting.LevelLightEngine;
public interface BlockAndLightGetter extends BlockGetter {
    default LevelLightEngine getLightEngine() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.BlockAndLightGetter#getLightEngine");
    }
    default int getBrightness(LightLayer layer, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.BlockAndLightGetter#getBrightness");
    }
    default int getRawBrightness(BlockPos pos, int darkening) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.BlockAndLightGetter#getRawBrightness");
    }
    default boolean canSeeSky(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.BlockAndLightGetter#canSeeSky");
    }
}
