// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.ItemStackWithSlot;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public class PlayerEnderChestContainer extends SimpleContainer {
    private final org.bukkit.inventory.Inventory backing;
    private EnderChestBlockEntity activeChest;

    public PlayerEnderChestContainer() {
        this(null);
    }

    public PlayerEnderChestContainer(org.bukkit.inventory.Inventory backing) {
        super(27);
        this.backing = backing;
    }

    public void setActiveChest(EnderChestBlockEntity activeChest) {
        this.activeChest = activeChest;
    }

    public boolean isActiveChest(EnderChestBlockEntity chest) {
        return activeChest == chest;
    }

    public void fromSlots(ValueInput.TypedInputList<ItemStackWithSlot> list) {
        for (ItemStackWithSlot entry : list) {
            if (entry.isValidInContainer(getContainerSize())) {
                setItem(entry.slot(), entry.stack());
            }
        }
    }

    public void storeAsSlots(ValueOutput.TypedOutputList<ItemStackWithSlot> output) {
        for (int slot = 0; slot < getContainerSize(); slot++) {
            ItemStack stack = getItem(slot);
            if (!stack.isEmpty()) {
                output.add(new ItemStackWithSlot(slot, stack));
            }
        }
    }

    public ItemStack getItem(int slot) {
        return backing == null ? super.getItem(slot) : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(backing.getItem(slot));
    }

    public void setItem(int slot, ItemStack itemStack) {
        if (backing == null) {
            super.setItem(slot, itemStack);
        } else {
            backing.setItem(slot, org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }

    public boolean stillValid(Player player) {
        return true;
    }

    public void startOpen(ContainerUser containerUser) {
        return;
    }

    public void stopOpen(ContainerUser containerUser) {
        return;
    }
}
