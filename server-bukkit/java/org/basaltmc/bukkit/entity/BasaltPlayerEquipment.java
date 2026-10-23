package org.basaltmc.bukkit.entity;

import org.basaltmc.bukkit.Unsupported;
import org.bukkit.entity.Entity;
import org.bukkit.inventory.EntityEquipment;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.PlayerInventory;

/** A player's worn and held items, which live in the inventory and drop when it does. */
public final class BasaltPlayerEquipment implements EntityEquipment {
    private final PlayerInventory inventory;
    private final Entity holder;

    public BasaltPlayerEquipment(PlayerInventory inventory, Entity holder) {
        this.inventory = inventory;
        this.holder = holder;
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item) {
        inventory.setItem(slot, item);
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item, boolean silent) {
        inventory.setItem(slot, item);
    }

    @Override
    public ItemStack getItem(EquipmentSlot slot) {
        return inventory.getItem(slot);
    }

    @Override
    public ItemStack getItemInMainHand() {
        return inventory.getItemInMainHand();
    }

    @Override
    public void setItemInMainHand(ItemStack item) {
        inventory.setItemInMainHand(item);
    }

    @Override
    public void setItemInMainHand(ItemStack item, boolean silent) {
        inventory.setItemInMainHand(item);
    }

    @Override
    public ItemStack getItemInOffHand() {
        return inventory.getItemInOffHand();
    }

    @Override
    public void setItemInOffHand(ItemStack item) {
        inventory.setItemInOffHand(item);
    }

    @Override
    public void setItemInOffHand(ItemStack item, boolean silent) {
        inventory.setItemInOffHand(item);
    }

    @Override
    public ItemStack getItemInHand() {
        return inventory.getItemInMainHand();
    }

    @Override
    public void setItemInHand(ItemStack item) {
        inventory.setItemInMainHand(item);
    }

    @Override
    public ItemStack getHelmet() {
        return inventory.getHelmet();
    }

    @Override
    public void setHelmet(ItemStack item) {
        inventory.setHelmet(item);
    }

    @Override
    public void setHelmet(ItemStack item, boolean silent) {
        inventory.setHelmet(item);
    }

    @Override
    public ItemStack getChestplate() {
        return inventory.getChestplate();
    }

    @Override
    public void setChestplate(ItemStack item) {
        inventory.setChestplate(item);
    }

    @Override
    public void setChestplate(ItemStack item, boolean silent) {
        inventory.setChestplate(item);
    }

    @Override
    public ItemStack getLeggings() {
        return inventory.getLeggings();
    }

    @Override
    public void setLeggings(ItemStack item) {
        inventory.setLeggings(item);
    }

    @Override
    public void setLeggings(ItemStack item, boolean silent) {
        inventory.setLeggings(item);
    }

    @Override
    public ItemStack getBoots() {
        return inventory.getBoots();
    }

    @Override
    public void setBoots(ItemStack item) {
        inventory.setBoots(item);
    }

    @Override
    public void setBoots(ItemStack item, boolean silent) {
        inventory.setBoots(item);
    }

    @Override
    public ItemStack[] getArmorContents() {
        return inventory.getArmorContents();
    }

    @Override
    public void setArmorContents(ItemStack[] items) {
        inventory.setArmorContents(items);
    }

    @Override
    public void clear() {
        inventory.clear();
    }

    @Override
    public float getDropChance(EquipmentSlot slot) {
        return 1.0f;
    }

    @Override
    public void setDropChance(EquipmentSlot slot, float chance) {
        throw Unsupported.hit("EntityEquipment#setDropChance on a player");
    }

    @Override
    public float getItemInHandDropChance() {
        return 1.0f;
    }

    @Override
    public void setItemInHandDropChance(float chance) {
        setDropChance(EquipmentSlot.HAND, chance);
    }

    @Override
    public float getItemInMainHandDropChance() {
        return 1.0f;
    }

    @Override
    public void setItemInMainHandDropChance(float chance) {
        setDropChance(EquipmentSlot.HAND, chance);
    }

    @Override
    public float getItemInOffHandDropChance() {
        return 1.0f;
    }

    @Override
    public void setItemInOffHandDropChance(float chance) {
        setDropChance(EquipmentSlot.OFF_HAND, chance);
    }

    @Override
    public float getHelmetDropChance() {
        return 1.0f;
    }

    @Override
    public void setHelmetDropChance(float chance) {
        setDropChance(EquipmentSlot.HEAD, chance);
    }

    @Override
    public float getChestplateDropChance() {
        return 1.0f;
    }

    @Override
    public void setChestplateDropChance(float chance) {
        setDropChance(EquipmentSlot.CHEST, chance);
    }

    @Override
    public float getLeggingsDropChance() {
        return 1.0f;
    }

    @Override
    public void setLeggingsDropChance(float chance) {
        setDropChance(EquipmentSlot.LEGS, chance);
    }

    @Override
    public float getBootsDropChance() {
        return 1.0f;
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
