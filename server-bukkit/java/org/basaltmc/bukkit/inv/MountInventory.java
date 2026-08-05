package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.entity.EntityCore;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

public class MountInventory extends AbstractInventory
        implements org.bukkit.inventory.HorseInventory, org.bukkit.inventory.LlamaInventory {
    private static final int kEquipment = 2;

    protected final EntityCore core;
    protected final Entity animal;

    public MountInventory(EntityCore core, Entity animal) {
        this.core = core;
        this.animal = animal;
    }

    private int chestSize() {
        if (!core.nbtFlag("ChestedHorse")) {
            return 0;
        }
        return switch (core.type.getKey().getKey()) {
            case "llama", "trader_llama" -> Math.max(1, Math.min(5, core.nbtInt("Strength", 1))) * 3;
            default -> 15;
        };
    }

    @Override
    public int getSize() {
        return kEquipment + chestSize();
    }

    private void check(int slot) {
        if (slot < 0 || slot >= getSize()) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + getSize());
        }
    }

    private Map<String, Object> saved() {
        byte[] stored = core.nbt("");
        Map<String, Object> read = stored == null ? null : Nbt.readNamed(stored);
        return read == null ? Nbt.compound() : read;
    }

    @Override
    public ItemStack getItem(int slot) {
        check(slot);
        if (slot < kEquipment) {
            ItemStack worn = ((org.bukkit.entity.LivingEntity) animal).getEquipment()
                .getItem(slot == 0 ? EquipmentSlot.SADDLE : EquipmentSlot.BODY);
            return worn == null || worn.getType().isAir() ? null : worn;
        }
        List<Object> items = Nbt.list(saved().get("Items"));
        for (Object entry : items == null ? List.of() : items) {
            Map<String, Object> item = Nbt.map(entry);
            if (item != null && Nbt.integer(item, "Slot", -1) == slot - kEquipment) {
                ItemStack stack = Convert.stack(item);
                return stack instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : stack;
            }
        }
        return null;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        check(slot);
        boolean empty = item == null || item.getType().isAir() || item.getAmount() <= 0;
        if (slot < kEquipment) {
            ((org.bukkit.entity.LivingEntity) animal).getEquipment().setItem(slot == 0 ? EquipmentSlot.SADDLE
                : EquipmentSlot.BODY, empty ? null : item);
            return;
        }
        List<Object> items = Nbt.list(saved().get("Items"));
        List<Object> kept = new ArrayList<>();
        for (Object entry : items == null ? List.of() : items) {
            Map<String, Object> existing = Nbt.map(entry);
            if (existing != null && Nbt.integer(existing, "Slot", -1) != slot - kEquipment) {
                kept.add(existing);
            }
        }
        if (!empty) {
            Map<String, Object> stored = Convert.stack(item);
            stored.put("Slot", (byte) (slot - kEquipment));
            kept.add(stored);
        }
        Map<String, Object> patch = Nbt.compound();
        patch.put("Items", kept);
        core.mergeNbt(Nbt.writeNamed(patch));
    }

    @Override
    public List<HumanEntity> getViewers() {
        List<HumanEntity> out = new ArrayList<>();
        for (org.bukkit.entity.Player player : Bukkit.getOnlinePlayers()) {
            if (TypedInventories.unwrap(player.getOpenInventory().getTopInventory()) instanceof MountInventory other
                && other.animal.getEntityId() == animal.getEntityId()) {
                out.add(player);
            }
        }
        return out;
    }

    @Override
    public InventoryType getType() {
        return InventoryType.CHEST;
    }

    @Override
    public InventoryHolder getHolder() {
        return (InventoryHolder) animal;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return getHolder();
    }

    @Override
    public Location getLocation() {
        return animal.getLocation();
    }

    @Override
    public ItemStack getSaddle() {
        return getItem(0);
    }

    @Override
    public void setSaddle(ItemStack saddle) {
        setItem(0, saddle);
    }

    @Override
    public ItemStack getArmor() {
        return getItem(1);
    }

    @Override
    public void setArmor(ItemStack armor) {
        setItem(1, armor);
    }

    @Override
    public ItemStack getDecor() {
        return getItem(1);
    }

    @Override
    public void setDecor(ItemStack decor) {
        setItem(1, decor);
    }
}
