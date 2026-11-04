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

public class BrewingStandBlockEntity extends BaseContainerBlockEntity implements net.minecraft.world.WorldlyContainer {
    public static final int FUEL_USES = 20;
    public static final int DATA_BREW_TIME = 0;
    public static final int DATA_FUEL_USES = 1;
    public static final int NUM_DATA_VALUES = 2;
    private static final int[] kSlotsForUp = {3};
    private static final int[] kSlotsForDown = {0, 1, 2, 3};
    private static final int[] kSlotsForSides = {0, 1, 2, 4};

    public BrewingStandBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("brewing_stand"), worldPosition, blockState);
    }

    public BrewingStandBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getBrewTime() {
        return nbt().getShortOr("BrewTime", (short) 0);
    }

    public int getFuel() {
        return nbt().getByteOr("Fuel", (byte) 0);
    }

    @Override
    public int getContainerSize() {
        return 5;
    }

    @Override
    protected Component getDefaultName() {
        return Component.translatable("container.brewing");
    }

    @Override
    protected net.minecraft.core.NonNullList<ItemStack> getItems() {
        return readItems(5);
    }

    @Override
    protected void setItems(net.minecraft.core.NonNullList<ItemStack> items) {
        writeItems(items);
    }

    @Override
    public int[] getSlotsForFace(Direction direction) {
        return direction == Direction.UP ? kSlotsForUp : direction == Direction.DOWN ? kSlotsForDown : kSlotsForSides;
    }

    @Override
    public boolean canPlaceItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
        return canPlaceItem(slot, itemStack);
    }

    @Override
    public boolean canTakeItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
        return slot != 3 || itemStack.is(net.minecraft.world.item.Items.GLASS_BOTTLE);
    }

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return new net.minecraft.world.inventory.BrewingStandMenu(containerId, inventory, this, new net.minecraft.world.inventory.SimpleContainerData(2));
    }
}
