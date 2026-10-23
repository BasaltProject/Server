package org.basaltmc.bukkit.entity;

import org.bukkit.entity.Entity;
import org.bukkit.inventory.EntityEquipment;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;

public final class BasaltEntityEquipment implements EntityEquipment {
    private static final float kDefaultDropChance = 0.085f;

    private final EntityCore core;
    private final Entity holder;

    BasaltEntityEquipment(EntityCore core, Entity holder) {
        this.core = core;
        this.holder = holder;
    }

    private static String key(EquipmentSlot slot) {
        return switch (slot) {
            case HAND -> "mainhand";
            case OFF_HAND -> "offhand";
            case FEET -> "feet";
            case LEGS -> "legs";
            case CHEST -> "chest";
            case HEAD -> "head";
            case BODY -> "body";
            case SADDLE -> "saddle";
        };
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item) {
        setItem(slot, item, false);
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item, boolean silent) {
        core.call("set_equipment", key(slot), EntityItems.toNbt(item));
    }

    @Override
    public ItemStack getItem(EquipmentSlot slot) {
        Object stored = core.call("equipment", key(slot));
        ItemStack stack = stored instanceof byte[] bytes ? EntityItems.fromNbt(bytes) : null;
        return stack == null ? new ItemStack(org.bukkit.Material.AIR) : stack;
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
        return new ItemStack[] {getBoots(), getLeggings(), getChestplate(), getHelmet()};
    }

    @Override
    public void setArmorContents(ItemStack[] items) {
        setBoots(items.length > 0 ? items[0] : null);
        setLeggings(items.length > 1 ? items[1] : null);
        setChestplate(items.length > 2 ? items[2] : null);
        setHelmet(items.length > 3 ? items[3] : null);
    }

    @Override
    public void clear() {
        for (EquipmentSlot slot : EquipmentSlot.values()) {
            setItem(slot, null);
        }
    }

    @Override
    public float getDropChance(EquipmentSlot slot) {
        Object chance = core.call("drop_chance", key(slot));
        return chance instanceof Number number ? number.floatValue() : kDefaultDropChance;
    }

    @Override
    public void setDropChance(EquipmentSlot slot, float chance) {
        core.call("set_drop_chance", key(slot), (double) chance);
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
}
