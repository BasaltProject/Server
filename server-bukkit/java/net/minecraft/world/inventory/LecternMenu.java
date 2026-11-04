// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class LecternMenu extends AbstractContainerMenu {
    public static final int BUTTON_PREV_PAGE = 1;
    public static final int BUTTON_NEXT_PAGE = 2;
    public static final int BUTTON_TAKE_BOOK = 3;
    public static final int BUTTON_PAGE_JUMP_RANGE_START = 100;
    private static final int kDataCount = 1;
    private final Container lectern;
    private final ContainerData lecternData;

    public LecternMenu(int containerId) {
        this(containerId, new SimpleContainer(1), new SimpleContainerData(kDataCount));
    }

    public LecternMenu(int containerId, Container lectern, ContainerData lecternData) {
        super(MenuType.LECTERN, containerId);
        checkContainerSize(lectern, 1);
        checkContainerDataCount(lecternData, kDataCount);
        this.lectern = lectern;
        this.lecternData = lecternData;
        addSlot(new Slot(lectern, 0, 0, 0) {
            @Override
            public void setChanged() {
                super.setChanged();
                slotsChanged(lectern);
            }
        });
        addDataSlots(lecternData);
    }

    @Override
    public boolean clickMenuButton(Player player, int buttonId) {
        if (buttonId >= BUTTON_PAGE_JUMP_RANGE_START) {
            lecternData.set(0, buttonId - BUTTON_PAGE_JUMP_RANGE_START);
            return true;
        }
        switch (buttonId) {
            case BUTTON_NEXT_PAGE -> lecternData.set(0, lecternData.get(0) + 1);
            case BUTTON_PREV_PAGE -> lecternData.set(0, lecternData.get(0) - 1);
            case BUTTON_TAKE_BOOK -> {
                ItemStack book = lectern.removeItemNoUpdate(0);
                lectern.setChanged();
                player.getInventory().add(book);
            }
            default -> {
                return false;
            }
        }
        return true;
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        return ItemStack.EMPTY;
    }

    @Override
    public void setData(int id, int value) {
        super.setData(id, value);
        broadcastChanges();
    }

    @Override
    public boolean stillValid(Player player) {
        return lectern.stillValid(player);
    }

    public ItemStack getBook() {
        return lectern.getItem(0);
    }

    public int getPage() {
        return lecternData.get(0);
    }
}
