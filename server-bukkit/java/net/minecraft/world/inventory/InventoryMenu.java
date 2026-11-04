// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import java.util.List;
import net.minecraft.resources.Identifier;
import net.minecraft.world.Container;
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
    public static final Identifier EMPTY_ARMOR_SLOT_HELMET = null;
    public static final Identifier EMPTY_ARMOR_SLOT_CHESTPLATE = null;
    public static final Identifier EMPTY_ARMOR_SLOT_LEGGINGS = null;
    public static final Identifier EMPTY_ARMOR_SLOT_BOOTS = null;
    public static final Identifier EMPTY_ARMOR_SLOT_SHIELD = null;
    public final boolean active = false;
    public InventoryMenu(Inventory inventory, boolean active, Player owner) {
        super((net.minecraft.world.inventory.MenuType) null, (int) 0, (int) 0, (int) 0);
    }
    public static boolean isHotbarSlot(int slot) {
        return slot >= 36 && slot < 45 || slot == 45;
    }
    public void slotsChanged(Container container) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.inventory.InventoryMenu#slotsChanged");
    }
    public void removed(Player player) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.inventory.InventoryMenu#removed");
    }
    public boolean stillValid(Player player) {
        return true;
    }
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.inventory.InventoryMenu#quickMoveStack");
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.inventory.InventoryMenu#owner");
    }
}
