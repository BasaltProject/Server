package org.basaltmc.bukkit;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.bukkit.Material;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.ItemMeta;
import org.bukkit.material.MaterialData;

/** An item stack that is only a type and a count: names, lore and enchantments are not carried yet. */
public final class BasaltItemStack extends ItemStack {
    private Material type;
    private int amount;

    BasaltItemStack(Material type, int amount) {
        super();
        this.type = type;
        this.amount = amount;
    }

    @Override
    public Material getType() {
        return type;
    }

    @Override
    public void setType(Material type) {
        this.type = type;
    }

    @Override
    public ItemStack withType(Material type) {
        return new BasaltItemStack(type, amount);
    }

    @Override
    public int getAmount() {
        return amount;
    }

    @Override
    public void setAmount(int amount) {
        this.amount = amount;
    }

    @Override
    public MaterialData getData() {
        return new MaterialData(type);
    }

    @Override
    public void setData(MaterialData data) {}

    @Override
    public void setDurability(short durability) {}

    @Override
    public short getDurability() {
        return 0;
    }

    @Override
    public int getMaxStackSize() {
        return 64;
    }

    @Override
    public boolean isEmpty() {
        return type.isAir() || amount <= 0;
    }

    @Override
    public boolean isSimilar(ItemStack other) {
        return other != null && other.getType() == type;
    }

    @Override
    public ItemStack clone() {
        return new BasaltItemStack(type, amount);
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof ItemStack stack && stack.getType() == type && stack.getAmount() == amount;
    }

    @Override
    public int hashCode() {
        return type.hashCode() * 31 + amount;
    }

    @Override
    public String toString() {
        return "ItemStack{" + type + " x " + amount + "}";
    }

    @Override
    public boolean containsEnchantment(Enchantment enchantment) {
        return false;
    }

    @Override
    public int getEnchantmentLevel(Enchantment enchantment) {
        return 0;
    }

    @Override
    public Map<Enchantment, Integer> getEnchantments() {
        return new HashMap<>();
    }

    @Override
    public void removeEnchantments() {}

    @Override
    public ItemMeta getItemMeta() {
        return null;
    }

    @Override
    public boolean hasItemMeta() {
        return false;
    }

    @Override
    public boolean setItemMeta(ItemMeta meta) {
        return false;
    }

    @Override
    public ItemStack asOne() {
        return new BasaltItemStack(type, 1);
    }

    @Override
    public ItemStack asQuantity(int quantity) {
        return new BasaltItemStack(type, quantity);
    }

    @Override
    public ItemStack add(int quantity) {
        amount += quantity;
        return this;
    }

    @Override
    public ItemStack subtract(int quantity) {
        amount -= quantity;
        return this;
    }

    @Override
    public String getTranslationKey() {
        return "item.minecraft." + type.getKey().getKey();
    }

    @Override
    public String translationKey() {
        return getTranslationKey();
    }

    @Override
    public Component displayName() {
        return Component.text(type.getKey().getKey());
    }

    @Override
    public List<String> getLore() {
        return null;
    }
}
