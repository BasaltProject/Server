// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.CombinerNative;

public class SmithingMenu extends ItemCombinerMenu {
    public static final int TEMPLATE_SLOT = 0;
    public static final int BASE_SLOT = 1;
    public static final int ADDITIONAL_SLOT = 2;
    public static final int RESULT_SLOT = 3;
    public static final int TEMPLATE_SLOT_X_PLACEMENT = 8;
    public static final int BASE_SLOT_X_PLACEMENT = 26;
    public static final int ADDITIONAL_SLOT_X_PLACEMENT = 44;
    public static final int SLOT_Y_PLACEMENT = 48;

    public SmithingMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, ContainerLevelAccess.NULL);
    }

    public SmithingMenu(int containerId, Inventory inventory, ContainerLevelAccess access) {
        super(MenuType.SMITHING, containerId, inventory, access, ItemCombinerMenuSlotDefinition.create()
            .withSlot(TEMPLATE_SLOT, TEMPLATE_SLOT_X_PLACEMENT, SLOT_Y_PLACEMENT, stack -> true)
            .withSlot(BASE_SLOT, BASE_SLOT_X_PLACEMENT, SLOT_Y_PLACEMENT, stack -> true)
            .withSlot(ADDITIONAL_SLOT, ADDITIONAL_SLOT_X_PLACEMENT, SLOT_Y_PLACEMENT, stack -> true)
            .withResultSlot(RESULT_SLOT, 98, SLOT_Y_PLACEMENT)
            .build());
    }

    @Override
    protected boolean isValidBlock(BlockState state) {
        return state.is(net.minecraft.world.level.block.Blocks.SMITHING_TABLE);
    }

    @Override
    protected boolean mayPickup(Player player, boolean hasItem) {
        return hasItem;
    }

    @Override
    protected void onTake(Player player, ItemStack carried) {
        shrinkInput(TEMPLATE_SLOT);
        shrinkInput(BASE_SLOT);
        shrinkInput(ADDITIONAL_SLOT);
    }

    private void shrinkInput(int slot) {
        ItemStack stack = inputSlots.getItem(slot);
        if (!stack.isEmpty()) {
            stack.shrink(1);
            inputSlots.setItem(slot, stack);
        }
    }

    @Override
    public void createResult() {
        ItemStack base = inputSlots.getItem(BASE_SLOT);
        ItemStack addition = inputSlots.getItem(ADDITIONAL_SLOT);
        resultSlots.setItem(0, base.isEmpty() || addition.isEmpty() ? ItemStack.EMPTY
            : CombinerNative.smithing(inputSlots.getItem(TEMPLATE_SLOT), base, addition));
        broadcastChanges();
    }

    @Override
    public boolean canTakeItemForPickAll(ItemStack carried, Slot target) {
        return target.container != resultSlots && super.canTakeItemForPickAll(carried, target);
    }
}
