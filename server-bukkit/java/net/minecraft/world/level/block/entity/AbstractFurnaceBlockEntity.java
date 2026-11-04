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

public abstract class AbstractFurnaceBlockEntity extends BaseContainerBlockEntity implements net.minecraft.world.WorldlyContainer {
    protected static final int SLOT_INPUT = 0;
    protected static final int SLOT_FUEL = 1;
    protected static final int SLOT_RESULT = 2;
    public static final int DATA_LIT_TIME = 0;
    public static final int DATA_LIT_DURATION = 1;
    public static final int DATA_COOKING_PROGRESS = 2;
    public static final int DATA_COOKING_TOTAL_TIME = 3;
    public static final int NUM_DATA_VALUES = 4;
    public static final int BURN_TIME_STANDARD = 200;
    public static final int BURN_COOL_SPEED = 2;
    private static final int[] kSlotsForUp = {0};
    private static final int[] kSlotsForDown = {2, 1};
    private static final int[] kSlotsForSides = {1};

    protected AbstractFurnaceBlockEntity(BlockEntityType<?> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    protected AbstractFurnaceBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public int getLitTimeRemaining() {
        return nbt().getShortOr("lit_time_remaining", (short) 0);
    }

    public int getLitTotalTime() {
        return nbt().getShortOr("lit_total_time", (short) 0);
    }

    public int getCookingTimeSpent() {
        return nbt().getShortOr("cooking_time_spent", (short) 0);
    }

    public int getCookingTotalTime() {
        return nbt().getShortOr("cooking_total_time", (short) 0);
    }

    public boolean isLit() {
        return getLitTimeRemaining() > 0;
    }

    @Override
    public int getContainerSize() {
        return 3;
    }

    @Override
    protected net.minecraft.core.NonNullList<ItemStack> getItems() {
        return readItems(3);
    }

    @Override
    protected void setItems(net.minecraft.core.NonNullList<ItemStack> items) {
        writeItems(items);
    }

    @Override
    public int[] getSlotsForFace(Direction direction) {
        return direction == Direction.DOWN ? kSlotsForDown : direction == Direction.UP ? kSlotsForUp : kSlotsForSides;
    }

    @Override
    public boolean canPlaceItem(int slot, ItemStack itemStack) {
        return slot != SLOT_RESULT;
    }

    @Override
    public boolean canPlaceItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
        return canPlaceItem(slot, itemStack);
    }

    @Override
    public boolean canTakeItemThroughFace(int slot, ItemStack itemStack, Direction direction) {
        return direction != Direction.DOWN || slot != SLOT_FUEL || itemStack.is(net.minecraft.world.item.Items.WATER_BUCKET)
            || itemStack.is(net.minecraft.world.item.Items.BUCKET);
    }

    protected final net.minecraft.world.inventory.ContainerData dataAccess = new net.minecraft.world.inventory.ContainerData() {
        @Override
        public int get(int dataId) {
            return switch (dataId) {
                case DATA_LIT_TIME -> getLitTimeRemaining();
                case DATA_LIT_DURATION -> getLitTotalTime();
                case DATA_COOKING_PROGRESS -> getCookingTimeSpent();
                case DATA_COOKING_TOTAL_TIME -> getCookingTotalTime();
                default -> 0;
            };
        }

        @Override
        public void set(int dataId, int value) {
        }

        @Override
        public int getCount() {
            return NUM_DATA_VALUES;
        }
    };

    protected abstract AbstractContainerMenu createFurnaceMenu(int containerId, Inventory inventory);

    @Override
    protected AbstractContainerMenu createMenu(int containerId, Inventory inventory) {
        return createFurnaceMenu(containerId, inventory);
    }
}
