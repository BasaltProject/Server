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

public class CrafterBlockEntity extends RandomizableContainerBlockEntity {
    public CrafterBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("crafter"), worldPosition, blockState);
    }

    public CrafterBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getCraftingTicksRemaining() {
        return nbt().getIntOr("crafting_ticks_remaining", 0);
    }

    public boolean isSlotDisabled(int slot) {
        for (int disabled : nbt().getIntArray("disabled_slots").orElse(new int[0])) {
            if (disabled == slot) {
                return true;
            }
        }
        return false;
    }

    @Override
    public int getContainerSize() {
        return 9;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.crafter");
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.DispenserMenu(containerId, inventory, this);
    }
}
