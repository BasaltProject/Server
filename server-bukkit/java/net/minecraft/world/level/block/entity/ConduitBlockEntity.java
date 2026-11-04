// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class ConduitBlockEntity extends BlockEntity {
    public ConduitBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("conduit"), worldPosition, blockState);
    }

    public ConduitBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public boolean isActive() {
        org.bukkit.block.Conduit conduit = bukkit(org.bukkit.block.Conduit.class);
        return conduit != null && conduit.isActive();
    }

    public int getRange() {
        org.bukkit.block.Conduit conduit = bukkit(org.bukkit.block.Conduit.class);
        return conduit == null ? 0 : conduit.getRange();
    }

    public boolean hasTarget() {
        org.bukkit.block.Conduit conduit = bukkit(org.bukkit.block.Conduit.class);
        return conduit != null && conduit.hasTarget();
    }
}
