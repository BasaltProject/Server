// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import java.util.List;
import java.util.OptionalInt;
import net.minecraft.core.NonNullList;
import net.minecraft.network.HashedStack;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
public abstract class AbstractContainerMenu {
    public static final int SLOT_CLICKED_OUTSIDE = -999;
    public static final int CONTAINER_CLICK_PRIMARY = 0;
    public static final int CONTAINER_CLICK_SECONDARY = 1;
    public static final int QUICKCRAFT_TYPE_CHARITABLE = 0;
    public static final int QUICKCRAFT_TYPE_GREEDY = 1;
    public static final int QUICKCRAFT_TYPE_CLONE = 2;
    public static final int QUICKCRAFT_HEADER_START = 0;
    public static final int QUICKCRAFT_HEADER_CONTINUE = 1;
    public static final int QUICKCRAFT_HEADER_END = 2;
    public static final int CARRIED_SLOT_SIZE = Integer.MAX_VALUE;
    public static final int SLOTS_PER_ROW = 9;
    public static final int SLOT_SIZE = 18;
    public final NonNullList<Slot> slots = NonNullList.create();
    public final int containerId;
    private final MenuType<?> menuType;
    private ItemStack carried = ItemStack.EMPTY;
    private int stateId;
    private final java.util.List<DataSlot> dataSlots = new java.util.ArrayList<>();
    private final java.util.List<ContainerListener> listeners = new java.util.ArrayList<>();
    protected AbstractContainerMenu(MenuType<?> menuType, int containerId) {
        this.menuType = menuType;
        this.containerId = containerId;
    }
    protected void addInventoryHotbarSlots(Container inventory, int left, int top) {
        for (int column = 0; column < 9; column++) {
            addSlot(new Slot(inventory, column, left + column * 18, top));
        }
    }
    protected void addInventoryExtendedSlots(Container inventory, int left, int top) {
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 9; column++) {
                addSlot(new Slot(inventory, column + (row + 1) * 9, left + column * 18, top + row * 18));
            }
        }
    }
    protected void addStandardInventorySlots(Container container, int left, int top) {
        addInventoryExtendedSlots(container, left, top);
        addInventoryHotbarSlots(container, left, top + 58);
    }
    protected static boolean stillValid(ContainerLevelAccess access, Player player, Block block) {
        return access.evaluate((level, pos) -> level.getBlockState(pos).is(block)
            && player.distanceToSqr(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5) <= 64.0, true);
    }
    public MenuType<?> getType() {
        if (menuType == null) {
            throw new UnsupportedOperationException("Unable to construct this menu by type");
        }
        return menuType;
    }
    protected static void checkContainerSize(Container container, int expected) {
        if (container.getContainerSize() < expected) {
            throw new IllegalArgumentException("Container size " + container.getContainerSize() + " is smaller than expected " + expected);
        }
    }
    protected static void checkContainerDataCount(ContainerData data, int expected) {
        if (data.getCount() < expected) {
            throw new IllegalArgumentException("Container data size " + data.getCount() + " is smaller than expected " + expected);
        }
    }
    public boolean isValidSlotIndex(int slotIndex) {
        return slotIndex == -1 || slotIndex == -999 || slotIndex >= 0 && slotIndex < slots.size();
    }
    protected Slot addSlot(Slot slot) {
        slot.index = slots.size();
        slots.add(slot);
        return slot;
    }
    protected DataSlot addDataSlot(DataSlot dataSlot) {
        dataSlots.add(dataSlot);
        return dataSlot;
    }
    protected void addDataSlots(ContainerData container) {
        for (int i = 0; i < container.getCount(); i++) {
            addDataSlot(DataSlot.forContainer(container, i));
        }
    }
    public void addSlotListener(ContainerListener listener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener);
        }
    }
    public void setSynchronizer(ContainerSynchronizer synchronizer) {

    }
    public void sendAllDataToRemote() {

    }
    public void removeSlotListener(ContainerListener listener) {
        listeners.remove(listener);
    }
    public NonNullList<ItemStack> getItems() {
        NonNullList<ItemStack> items = NonNullList.withSize(slots.size(), ItemStack.EMPTY);
        for (int i = 0; i < slots.size(); i++) {
            items.set(i, slots.get(i).getItem());
        }
        return items;
    }
    public void broadcastChanges() {

    }
    public void broadcastFullState() {

    }
    public void setRemoteSlot(int slot, ItemStack itemStack) {

    }
    public void setRemoteSlotUnsafe(int slot, HashedStack itemStack) {
        return;
    }
    public void setRemoteCarried(HashedStack carriedItem) {

    }
    public boolean clickMenuButton(Player player, int buttonId) {
        return false;
    }
    public Slot getSlot(int index) {
        return slots.get(index);
    }
    public abstract ItemStack quickMoveStack(Player player, int slotIndex);
    public void setSelectedBundleItemIndex(int slotIndex, int selectedItemIndex) {
        if (slotIndex >= 0 && slotIndex < slots.size()) {
            net.minecraft.world.item.BundleItem.toggleSelectedItem(slots.get(slotIndex).getItem(), selectedItemIndex);
        }
    }
    public void clicked(int slotIndex, int buttonNum, ContainerInput containerInput, Player player) {
        if (containerInput == ContainerInput.PICKUP && slotIndex >= 0 && slotIndex < slots.size()) {
            Slot slot = slots.get(slotIndex);
            ItemStack inSlot = slot.getItem();
            if (carried.isEmpty()) {
                if (!inSlot.isEmpty() && slot.mayPickup(player)) {
                    int take = buttonNum == 0 ? inSlot.getCount() : (inSlot.getCount() + 1) / 2;
                    carried = slot.remove(take);
                    slot.onTake(player, carried);
                }
            } else if (slot.mayPlace(carried)) {
                if (inSlot.isEmpty()) {
                    int put = buttonNum == 0 ? carried.getCount() : 1;
                    slot.setByPlayer(carried.split(put));
                } else if (ItemStack.isSameItemSameComponents(inSlot, carried)) {
                    int room = slot.getMaxStackSize(carried) - inSlot.getCount();
                    int put = Math.min(buttonNum == 0 ? carried.getCount() : 1, room);
                    carried.shrink(put);
                    inSlot.grow(put);
                    slot.setChanged();
                } else if (carried.getCount() <= slot.getMaxStackSize(carried) && slot.mayPickup(player)) {
                    slot.setByPlayer(carried);
                    carried = inSlot;
                }
            }
        } else if (containerInput == ContainerInput.QUICK_MOVE && slotIndex >= 0 && slotIndex < slots.size()) {
            ItemStack moved = quickMoveStack(player, slotIndex);
            while (!moved.isEmpty() && ItemStack.isSameItem(slots.get(slotIndex).getItem(), moved)) {
                moved = quickMoveStack(player, slotIndex);
            }
        }
        broadcastChanges();
    }
    public boolean canTakeItemForPickAll(ItemStack carried, Slot target) {
        return true;
    }
    public void removed(Player player) {

    }
    protected void clearContainer(Player player, Container container) {
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            ItemStack stack = container.removeItemNoUpdate(slot);
            if (!stack.isEmpty() && !player.getInventory().add(stack)) {
                player.drop(stack, false, null);
            }
        }
    }
    public void slotsChanged(Container container) {
        broadcastChanges();
    }
    public void setItem(int slot, int stateId, ItemStack itemStack) {
        getSlot(slot).set(itemStack);
        this.stateId = stateId;
    }
    public void initializeContents(int stateId, List<ItemStack> items, ItemStack carried) {
        for (int i = 0; i < items.size(); i++) {
            getSlot(i).set(items.get(i));
        }
        this.carried = carried;
        this.stateId = stateId;
    }
    public void setData(int id, int value) {
        dataSlots.get(id).set(value);
    }
    public abstract boolean stillValid(Player player);
    protected boolean moveItemStackTo(ItemStack itemStack, int startSlot, int endSlot, boolean backwards) {
        boolean changed = false;
        int index = backwards ? endSlot - 1 : startSlot;
        if (itemStack.getMaxStackSize() > 1) {
            while (!itemStack.isEmpty() && (backwards ? index >= startSlot : index < endSlot)) {
                Slot slot = slots.get(index);
                ItemStack target = slot.getItem();
                if (!target.isEmpty() && ItemStack.isSameItemSameComponents(itemStack, target)) {
                    int total = target.getCount() + itemStack.getCount();
                    int limit = slot.getMaxStackSize(target);
                    if (total <= limit) {
                        itemStack.setCount(0);
                        target.setCount(total);
                        slot.setChanged();
                        changed = true;
                    } else if (target.getCount() < limit) {
                        itemStack.shrink(limit - target.getCount());
                        target.setCount(limit);
                        slot.setChanged();
                        changed = true;
                    }
                }
                index += backwards ? -1 : 1;
            }
        }
        if (!itemStack.isEmpty()) {
            index = backwards ? endSlot - 1 : startSlot;
            while (backwards ? index >= startSlot : index < endSlot) {
                Slot slot = slots.get(index);
                if (!slot.hasItem() && slot.mayPlace(itemStack)) {
                    int limit = slot.getMaxStackSize(itemStack);
                    slot.setByPlayer(itemStack.split(Math.min(itemStack.getCount(), limit)));
                    slot.setChanged();
                    changed = true;
                    break;
                }
                index += backwards ? -1 : 1;
            }
        }
        return changed;
    }
    public static int getQuickcraftType(int mask) {
        return mask >> 2 & 3;
    }
    public static int getQuickcraftHeader(int mask) {
        return mask & 3;
    }
    public static int getQuickcraftMask(int header, int type) {
        return header & 3 | (type & 3) << 2;
    }
    public static boolean isValidQuickcraftType(int type, Player player) {
        return type == 0 || type == 1 || type == 2 && player != null;
    }
    protected void resetQuickCraft() {

    }
    public static boolean canItemQuickReplace(Slot slot, ItemStack itemStack, boolean ignoreSize) {
        boolean empty = slot == null || !slot.hasItem();
        return !empty && ItemStack.isSameItemSameComponents(itemStack, slot.getItem())
            ? slot.getItem().getCount() + (ignoreSize ? 0 : itemStack.getCount()) <= itemStack.getMaxStackSize()
            : empty;
    }
    public static int getQuickCraftPlaceCount(int quickCraftSlotsSize, int quickCraftingType, ItemStack itemStack) {
        return switch (quickCraftingType) {
            case 0 -> Math.floorDiv(itemStack.getCount(), quickCraftSlotsSize);
            case 1 -> 1;
            case 2 -> itemStack.getMaxStackSize();
            default -> itemStack.getCount();
        };
    }
    public boolean canDragTo(Slot slot) {
        return true;
    }
    public static int getRedstoneSignalFromBlockEntity(BlockEntity blockEntity) {
        return blockEntity instanceof Container container ? getRedstoneSignalFromContainer(container) : 0;
    }
    public static int getRedstoneSignalFromContainer(Container container) {
        if (container == null) {
            return 0;
        }
        float fullness = 0.0F;
        for (int i = 0; i < container.getContainerSize(); i++) {
            ItemStack stack = container.getItem(i);
            if (!stack.isEmpty()) {
                fullness += (float) stack.getCount() / Math.min(container.getMaxStackSize(), stack.getMaxStackSize());
            }
        }
        fullness /= container.getContainerSize();
        return (int) Math.floor(fullness * 14.0F) + (fullness > 0.0F ? 1 : 0);
    }
    public void setCarried(ItemStack carried) {
        this.carried = carried;
    }
    public ItemStack getCarried() {
        return carried;
    }
    public void suppressRemoteUpdates() {

    }
    public void resumeRemoteUpdates() {

    }
    public void transferState(AbstractContainerMenu otherContainer) {
        for (int i = 0; i < otherContainer.slots.size() && i < slots.size(); i++) {
            slots.get(i).set(otherContainer.slots.get(i).getItem());
        }
        carried = otherContainer.carried;
    }
    public OptionalInt findSlot(Container inventory, int slotIndex) {
        for (int i = 0; i < slots.size(); i++) {
            if (slots.get(i).container == inventory && slots.get(i).getContainerSlot() == slotIndex) {
                return java.util.OptionalInt.of(i);
            }
        }
        return java.util.OptionalInt.empty();
    }
    public int getStateId() {
        return stateId;
    }
    public int incrementStateId() {
        stateId = stateId + 1 & 32767;
        return stateId;
    }
}
