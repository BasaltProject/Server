// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class DaylightDetectorBlockEntity extends BlockEntity {
    public DaylightDetectorBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("daylight_detector"), worldPosition, blockState);
    }

    public DaylightDetectorBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
}
