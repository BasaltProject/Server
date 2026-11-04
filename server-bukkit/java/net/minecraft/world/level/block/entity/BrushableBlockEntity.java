// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class BrushableBlockEntity extends BlockEntity {
    public BrushableBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("brushable_block"), worldPosition, blockState);
    }

    public BrushableBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public net.minecraft.world.item.ItemStack getItem() {
        return net.minecraft.world.item.ItemStack.OPTIONAL_CODEC.parse(net.minecraft.nbt.NbtOps.INSTANCE, nbt().getCompoundOrEmpty("item"))
            .result().orElse(net.minecraft.world.item.ItemStack.EMPTY);
    }
}
