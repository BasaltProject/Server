// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class EnderChestBlockEntity extends BlockEntity implements LidBlockEntity {
    public EnderChestBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("ender_chest"), worldPosition, blockState);
    }

    public EnderChestBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public float getOpenNess(float a) {
        return org.basaltmc.bukkit.facade.LidControl.openness(this);
    }

    public void startOpen(net.minecraft.world.entity.ContainerUser user) {
        org.basaltmc.bukkit.facade.LidControl.set(this, true);
    }

    public void stopOpen(net.minecraft.world.entity.ContainerUser user) {
        org.basaltmc.bukkit.facade.LidControl.set(this, false);
    }
}
