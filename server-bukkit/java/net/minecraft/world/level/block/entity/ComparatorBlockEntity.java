// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class ComparatorBlockEntity extends BlockEntity {
    public ComparatorBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("comparator"), worldPosition, blockState);
    }

    public ComparatorBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getOutputSignal() {
        return nbt().getIntOr("OutputSignal", 0);
    }

    public void setOutputSignal(int outputSignal) {
        TileNbt.update(this, tag -> tag.putInt("OutputSignal", outputSignal));
    }
}
