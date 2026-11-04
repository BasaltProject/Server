// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import org.basaltmc.bukkit.item.ItemData;

public class ResultSlot extends Slot {
    private final CraftingContainer craftSlots;
    private final Player player;
    private int removeCount;

    public ResultSlot(Player player, CraftingContainer craftSlots, Container container, int id, int x, int y) {
        super(container, id, x, y);
        this.player = player;
        this.craftSlots = craftSlots;
    }

    @Override
    public boolean mayPlace(ItemStack itemStack) {
        return false;
    }

    @Override
    public ItemStack remove(int amount) {
        if (hasItem()) {
            removeCount += Math.min(amount, getItem().getCount());
        }
        return super.remove(amount);
    }

    @Override
    protected void onQuickCraft(ItemStack picked, int count) {
        removeCount += count;
        checkTakeAchievements(picked);
    }

    @Override
    protected void onSwapCraft(int count) {
        removeCount += count;
    }

    @Override
    public ItemStack safeClone(Player player) {
        return getItem().copy();
    }

    @Override
    protected void checkTakeAchievements(ItemStack carried) {
        if (removeCount > 0) {
            carried.onCraftedBy(player, removeCount);
        }
        removeCount = 0;
    }

    @Override
    public void onTake(Player player, ItemStack carried) {
        checkTakeAchievements(carried);
        for (int i = 0; i < craftSlots.getContainerSize(); i++) {
            ItemStack ingredient = craftSlots.getItem(i);
            if (ingredient.isEmpty()) {
                continue;
            }
            String remainderKey = ItemData.of(ingredient.getItem().itemKey().toString()).craftingRemainder();
            ingredient.shrink(1);
            if (remainderKey == null) {
                continue;
            }
            ItemStack remainder = new ItemStack(BuiltInRegistries.ITEM.getValue(Identifier.parse(remainderKey)), 1);
            if (ingredient.isEmpty()) {
                craftSlots.setItem(i, remainder);
            } else if (!player.getInventory().add(remainder)) {
                ((org.bukkit.entity.Player) player.getBukkitEntity()).getWorld().dropItem(
                    ((org.bukkit.entity.Player) player.getBukkitEntity()).getLocation(), org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(remainder));
            }
        }
        craftSlots.setChanged();
    }

    @Override
    public boolean isFake() {
        return false;
    }
}
