// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.CombinerNative;

public class AnvilMenu extends ItemCombinerMenu {
    public static final int INPUT_SLOT = 0;
    public static final int ADDITIONAL_SLOT = 1;
    public static final int RESULT_SLOT = 2;
    public static final int MAX_NAME_LENGTH = 50;
    private static final int kMaxCost = 40;
    private final DataSlot cost = DataSlot.standalone();
    private int repairItemCountCost;
    private String itemName;

    public AnvilMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, ContainerLevelAccess.NULL);
    }

    public AnvilMenu(int containerId, Inventory inventory, ContainerLevelAccess access) {
        super(MenuType.ANVIL, containerId, inventory, access, ItemCombinerMenuSlotDefinition.create()
            .withSlot(INPUT_SLOT, 27, 47, stack -> true)
            .withSlot(ADDITIONAL_SLOT, 76, 47, stack -> true)
            .withResultSlot(RESULT_SLOT, 134, 47)
            .build());
        addDataSlot(cost);
    }

    @Override
    protected boolean isValidBlock(BlockState state) {
        return state.is(net.minecraft.world.level.block.Blocks.ANVIL) || state.is(net.minecraft.world.level.block.Blocks.CHIPPED_ANVIL)
            || state.is(net.minecraft.world.level.block.Blocks.DAMAGED_ANVIL);
    }

    @Override
    protected boolean mayPickup(Player player, boolean hasItem) {
        return (player.hasInfiniteMaterials() || player.experienceLevel >= cost.get()) && cost.get() > 0 && hasItem;
    }

    @Override
    protected void onTake(Player player, ItemStack carried) {
        if (!player.hasInfiniteMaterials()) {
            player.giveExperienceLevels(-cost.get());
        }
        if (repairItemCountCost > 0) {
            ItemStack addition = inputSlots.getItem(ADDITIONAL_SLOT);
            if (!addition.isEmpty() && addition.getCount() > repairItemCountCost) {
                addition.shrink(repairItemCountCost);
                inputSlots.setItem(ADDITIONAL_SLOT, addition);
            } else {
                inputSlots.setItem(ADDITIONAL_SLOT, ItemStack.EMPTY);
            }
        } else {
            inputSlots.setItem(ADDITIONAL_SLOT, ItemStack.EMPTY);
        }
        cost.set(0);
        inputSlots.setItem(INPUT_SLOT, ItemStack.EMPTY);
    }

    @Override
    public void createResult() {
        ItemStack input = inputSlots.getItem(INPUT_SLOT);
        if (input.isEmpty()) {
            resultSlots.setItem(0, ItemStack.EMPTY);
            cost.set(0);
            return;
        }
        CombinerNative.Anvil made = CombinerNative.anvil(input, inputSlots.getItem(ADDITIONAL_SLOT), itemName);
        repairItemCountCost = made.materials();
        boolean tooExpensive = made.cost() >= kMaxCost && !player.hasInfiniteMaterials();
        resultSlots.setItem(0, tooExpensive ? ItemStack.EMPTY : made.result());
        cost.set(made.result().isEmpty() ? 0 : made.cost());
        broadcastChanges();
    }

    public static int calculateIncreasedRepairCost(int baseCost) {
        return (int) Math.min(baseCost * 2L + 1L, Integer.MAX_VALUE);
    }

    public boolean setItemName(String name) {
        String filtered = name == null ? "" : name;
        if (filtered.length() > MAX_NAME_LENGTH) {
            return false;
        }
        String next = filtered.isEmpty() ? null : filtered;
        if (java.util.Objects.equals(next, itemName)) {
            return false;
        }
        itemName = next;
        if (getSlot(RESULT_SLOT).hasItem() || !inputSlots.getItem(INPUT_SLOT).isEmpty()) {
            createResult();
        }
        return true;
    }

    public int getCost() {
        return cost.get();
    }
}
