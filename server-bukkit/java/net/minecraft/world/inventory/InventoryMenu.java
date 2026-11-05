// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import java.util.List;
import net.minecraft.resources.Identifier;
import net.minecraft.world.Container;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
public class InventoryMenu extends AbstractCraftingMenu {
    public static final int CONTAINER_ID = 0;
    public static final int RESULT_SLOT = 0;
    public static final int CRAFT_SLOT_START = 1;
    public static final int CRAFT_SLOT_COUNT = 4;
    public static final int CRAFT_SLOT_END = 5;
    public static final int ARMOR_SLOT_START = 5;
    public static final int ARMOR_SLOT_COUNT = 4;
    public static final int ARMOR_SLOT_END = 9;
    public static final int INV_SLOT_START = 9;
    public static final int INV_SLOT_END = 36;
    public static final int USE_ROW_SLOT_START = 36;
    public static final int USE_ROW_SLOT_END = 45;
    public static final int SHIELD_SLOT = 45;
    public static final Identifier EMPTY_ARMOR_SLOT_HELMET = Identifier.withDefaultNamespace("container/slot/helmet");
    public static final Identifier EMPTY_ARMOR_SLOT_CHESTPLATE = Identifier.withDefaultNamespace("container/slot/chestplate");
    public static final Identifier EMPTY_ARMOR_SLOT_LEGGINGS = Identifier.withDefaultNamespace("container/slot/leggings");
    public static final Identifier EMPTY_ARMOR_SLOT_BOOTS = Identifier.withDefaultNamespace("container/slot/boots");
    public static final Identifier EMPTY_ARMOR_SLOT_SHIELD = Identifier.withDefaultNamespace("container/slot/shield");
    public final boolean active;
    private final Player owner;
    public InventoryMenu(Inventory inventory, boolean active, Player owner) {
        super(null, 0, 2, 2);
        this.active = active;
        this.owner = owner;
        addResultSlot(owner, 154, 28);
        addCraftingGridSlots(98, 18);
        EquipmentSlot[] order = {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET};
        Identifier[] icons = {EMPTY_ARMOR_SLOT_HELMET, EMPTY_ARMOR_SLOT_CHESTPLATE, EMPTY_ARMOR_SLOT_LEGGINGS, EMPTY_ARMOR_SLOT_BOOTS};
        for (int i = 0; i < 4; i++) {
            addSlot(new ArmorSlot(inventory, owner, order[i], 39 - i, 8, 8 + i * 18, icons[i]));
        }
        addStandardInventorySlots(inventory, 8, 84);
        addSlot(new Slot(inventory, Inventory.SLOT_OFFHAND, 77, 62) {
            @Override
            public void setByPlayer(ItemStack stack, ItemStack previous) {
                set(stack);
            }
            @Override
            public Identifier getNoItemIcon() {
                return EMPTY_ARMOR_SLOT_SHIELD;
            }
        });
    }
    public static boolean isHotbarSlot(int slot) {
        return slot >= 36 && slot < 45 || slot == 45;
    }
    public void slotsChanged(Container container) {
        super.slotsChanged(container);
    }
    public void removed(Player player) {
        super.removed(player);
        resultSlots.clearContent();
        clearContainer(player, craftSlots);
    }
    public boolean stillValid(Player player) {
        return true;
    }
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        EquipmentSlot target = player.getEquipmentSlotForItem(stack);
        boolean moved;
        if (slotIndex == RESULT_SLOT) {
            moved = moveItemStackTo(stack, INV_SLOT_START, USE_ROW_SLOT_END, true);
            if (moved) {
                slot.onQuickCraft(stack, original);
            }
        } else if (slotIndex < ARMOR_SLOT_END) {
            moved = moveItemStackTo(stack, INV_SLOT_START, USE_ROW_SLOT_END, false);
        } else if (target.getType() == EquipmentSlot.Type.HUMANOID_ARMOR && !slots.get(ARMOR_SLOT_START + 3 - target.getIndex()).hasItem()) {
            moved = moveItemStackTo(stack, ARMOR_SLOT_START + 3 - target.getIndex(), ARMOR_SLOT_START + 3 - target.getIndex() + 1, false);
        } else if (target == EquipmentSlot.OFFHAND && !slots.get(SHIELD_SLOT).hasItem()) {
            moved = moveItemStackTo(stack, SHIELD_SLOT, SHIELD_SLOT + 1, false);
        } else if (slotIndex < INV_SLOT_END) {
            moved = moveItemStackTo(stack, USE_ROW_SLOT_START, USE_ROW_SLOT_END, false);
        } else {
            moved = moveItemStackTo(stack, INV_SLOT_START, INV_SLOT_END, false);
        }
        if (!moved) {
            return ItemStack.EMPTY;
        }
        if (stack.isEmpty()) {
            slot.setByPlayer(ItemStack.EMPTY, original);
        } else {
            slot.setChanged();
        }
        return stack.getCount() == original.getCount() ? ItemStack.EMPTY : original;
    }
    public boolean canTakeItemForPickAll(ItemStack carried, Slot target) {
        return target.container != resultSlots && super.canTakeItemForPickAll(carried, target);
    }
    public Slot getResultSlot() {
        return slots.get(0);
    }
    public List<Slot> getInputGridSlots() {
        return slots.subList(1, 5);
    }
    public CraftingContainer getCraftSlots() {
        return craftSlots;
    }
    public RecipeBookType getRecipeBookType() {
        return RecipeBookType.CRAFTING;
    }
    protected Player owner() {
        return owner;
    }
}
