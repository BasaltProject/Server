package org.basaltmc.bukkit.inv;

import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.inventory.EntityEquipment;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.PlayerInventory;

public final class BasaltEntityEquipment implements EntityEquipment {
    public interface Access {
        ItemStack get(EquipmentSlot slot);

        void set(EquipmentSlot slot, ItemStack item, boolean silent);

        float dropChance(EquipmentSlot slot);

        void setDropChance(EquipmentSlot slot, float chance);
    }

    private static final EquipmentSlot[] kArmor = {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET};

    private final Entity holder;
    private final Access access;

    public BasaltEntityEquipment(Entity holder, Access access) {
        this.holder = holder;
        this.access = access;
    }

    /** A player's equipment is their inventory's armour and hands, which never drop on their own. */
    public static BasaltEntityEquipment of(Player player) {
        PlayerInventory inventory = player.getInventory();
        return new BasaltEntityEquipment(player, new Access() {
            @Override
            public ItemStack get(EquipmentSlot slot) {
                return inventory.getItem(slot);
            }

            @Override
            public void set(EquipmentSlot slot, ItemStack item, boolean silent) {
                inventory.setItem(slot, item);
            }

            @Override
            public float dropChance(EquipmentSlot slot) {
                return 1.0F;
            }

            @Override
            public void setDropChance(EquipmentSlot slot, float chance) {
                throw new UnsupportedOperationException("A player's equipment has no drop chance");
            }
        });
    }

    private static ItemStack nonNull(ItemStack item) {
        return item == null ? ItemStack.empty() : item;
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item) {
        setItem(slot, item, false);
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item, boolean silent) {
        access.set(slot, item, silent);
    }

    @Override
    public ItemStack getItem(EquipmentSlot slot) {
        return nonNull(access.get(slot));
    }

    @Override
    public ItemStack getItemInMainHand() {
        return getItem(EquipmentSlot.HAND);
    }

    @Override
    public void setItemInMainHand(ItemStack item) {
        setItem(EquipmentSlot.HAND, item);
    }

    @Override
    public void setItemInMainHand(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.HAND, item, silent);
    }

    @Override
    public ItemStack getItemInOffHand() {
        return getItem(EquipmentSlot.OFF_HAND);
    }

    @Override
    public void setItemInOffHand(ItemStack item) {
        setItem(EquipmentSlot.OFF_HAND, item);
    }

    @Override
    public void setItemInOffHand(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.OFF_HAND, item, silent);
    }

    @Override
    public ItemStack getItemInHand() {
        return getItemInMainHand();
    }

    @Override
    public void setItemInHand(ItemStack item) {
        setItemInMainHand(item);
    }

    @Override
    public ItemStack getHelmet() {
        return getItem(EquipmentSlot.HEAD);
    }

    @Override
    public void setHelmet(ItemStack item) {
        setItem(EquipmentSlot.HEAD, item);
    }

    @Override
    public void setHelmet(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.HEAD, item, silent);
    }

    @Override
    public ItemStack getChestplate() {
        return getItem(EquipmentSlot.CHEST);
    }

    @Override
    public void setChestplate(ItemStack item) {
        setItem(EquipmentSlot.CHEST, item);
    }

    @Override
    public void setChestplate(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.CHEST, item, silent);
    }

    @Override
    public ItemStack getLeggings() {
        return getItem(EquipmentSlot.LEGS);
    }

    @Override
    public void setLeggings(ItemStack item) {
        setItem(EquipmentSlot.LEGS, item);
    }

    @Override
    public void setLeggings(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.LEGS, item, silent);
    }

    @Override
    public ItemStack getBoots() {
        return getItem(EquipmentSlot.FEET);
    }

    @Override
    public void setBoots(ItemStack item) {
        setItem(EquipmentSlot.FEET, item);
    }

    @Override
    public void setBoots(ItemStack item, boolean silent) {
        setItem(EquipmentSlot.FEET, item, silent);
    }

    @Override
    public ItemStack[] getArmorContents() {
        ItemStack[] armor = new ItemStack[kArmor.length];
        for (int at = 0; at < kArmor.length; at++) {
            armor[at] = getItem(kArmor[at]);
        }
        return new ItemStack[] {armor[3], armor[2], armor[1], armor[0]};
    }

    @Override
    public void setArmorContents(ItemStack[] items) {
        EquipmentSlot[] bootsFirst = {EquipmentSlot.FEET, EquipmentSlot.LEGS, EquipmentSlot.CHEST, EquipmentSlot.HEAD};
        for (int at = 0; at < bootsFirst.length; at++) {
            setItem(bootsFirst[at], at < items.length ? items[at] : null);
        }
    }

    @Override
    public void clear() {
        for (EquipmentSlot slot : new EquipmentSlot[] {EquipmentSlot.HAND, EquipmentSlot.OFF_HAND, EquipmentSlot.HEAD,
            EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET}) {
            setItem(slot, null);
        }
    }

    @Override
    public float getItemInHandDropChance() {
        return getDropChance(EquipmentSlot.HAND);
    }

    @Override
    public void setItemInHandDropChance(float chance) {
        setDropChance(EquipmentSlot.HAND, chance);
    }

    @Override
    public float getItemInMainHandDropChance() {
        return getDropChance(EquipmentSlot.HAND);
    }

    @Override
    public void setItemInMainHandDropChance(float chance) {
        setDropChance(EquipmentSlot.HAND, chance);
    }

    @Override
    public float getItemInOffHandDropChance() {
        return getDropChance(EquipmentSlot.OFF_HAND);
    }

    @Override
    public void setItemInOffHandDropChance(float chance) {
        setDropChance(EquipmentSlot.OFF_HAND, chance);
    }

    @Override
    public float getHelmetDropChance() {
        return getDropChance(EquipmentSlot.HEAD);
    }

    @Override
    public void setHelmetDropChance(float chance) {
        setDropChance(EquipmentSlot.HEAD, chance);
    }

    @Override
    public float getChestplateDropChance() {
        return getDropChance(EquipmentSlot.CHEST);
    }

    @Override
    public void setChestplateDropChance(float chance) {
        setDropChance(EquipmentSlot.CHEST, chance);
    }

    @Override
    public float getLeggingsDropChance() {
        return getDropChance(EquipmentSlot.LEGS);
    }

    @Override
    public void setLeggingsDropChance(float chance) {
        setDropChance(EquipmentSlot.LEGS, chance);
    }

    @Override
    public float getBootsDropChance() {
        return getDropChance(EquipmentSlot.FEET);
    }

    @Override
    public void setBootsDropChance(float chance) {
        setDropChance(EquipmentSlot.FEET, chance);
    }

    @Override
    public Entity getHolder() {
        return holder;
    }

    @Override
    public float getDropChance(EquipmentSlot slot) {
        return access.dropChance(slot);
    }

    @Override
    public void setDropChance(EquipmentSlot slot, float chance) {
        access.setDropChance(slot, chance);
    }
}
