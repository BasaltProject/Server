package org.basaltmc.bukkit.item;

import io.papermc.paper.enchantments.EnchantmentRarity;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.set.RegistryKeySet;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import org.basaltmc.bukkit.InventoryNatives;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.enchantments.EnchantmentTarget;
import org.bukkit.entity.EntityCategory;
import org.bukkit.entity.EntityType;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.EquipmentSlotGroup;
import org.bukkit.inventory.ItemStack;

/** An enchantment as the datapack defines it: levels, costs, exclusivity and the items it fits. */
public final class BasaltEnchantment extends Enchantment {
    private final NamespacedKey key;
    private Map<String, Object> definition;
    private Set<String> supported;
    private Set<String> exclusive;

    public BasaltEnchantment(NamespacedKey key) {
        this.key = key;
    }

    private synchronized Map<String, Object> definition() {
        if (definition == null) {
            byte[] bytes = InventoryNatives.enchantmentData(key.toString());
            Map<String, Object> read = bytes == null ? null : Nbt.read(bytes);
            definition = read == null ? Nbt.compound() : read;
        }
        return definition;
    }

    private synchronized Set<String> supportedIds() {
        if (supported == null) {
            supported = TagSets.resolve("items", definition().get("supported_items"));
        }
        return supported;
    }

    private synchronized Set<String> exclusiveIds() {
        if (exclusive == null) {
            exclusive = TagSets.resolve("enchantment", definition().get("exclusive_set"));
        }
        return exclusive;
    }

    private boolean inTag(String tag) {
        return TagSets.members("enchantment", "minecraft:" + tag).contains(key.toString());
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public String getName() {
        return key.getKey().toUpperCase(java.util.Locale.ROOT);
    }

    @Override
    public int getMaxLevel() {
        return Nbt.integer(definition(), "max_level", 1);
    }

    @Override
    public int getStartLevel() {
        return 1;
    }

    @Override
    public EnchantmentTarget getItemTarget() {
        Object spec = definition().get("supported_items");
        String tag = spec instanceof String text ? text : "";
        switch (tag.replace("#minecraft:enchantable/", "")) {
            case "foot_armor":
                return EnchantmentTarget.ARMOR_FEET;
            case "leg_armor":
                return EnchantmentTarget.ARMOR_LEGS;
            case "chest_armor":
                return EnchantmentTarget.ARMOR_TORSO;
            case "head_armor":
                return EnchantmentTarget.ARMOR_HEAD;
            case "armor":
                return EnchantmentTarget.ARMOR;
            case "sword", "melee_weapon", "sharp_weapon", "weapon":
                return EnchantmentTarget.WEAPON;
            case "mining", "mining_loot":
                return EnchantmentTarget.TOOL;
            case "bow":
                return EnchantmentTarget.BOW;
            case "crossbow":
                return EnchantmentTarget.CROSSBOW;
            case "trident":
                return EnchantmentTarget.TRIDENT;
            case "fishing":
                return EnchantmentTarget.FISHING_ROD;
            case "durability":
                return EnchantmentTarget.BREAKABLE;
            case "equippable":
                return EnchantmentTarget.WEARABLE;
            case "vanishing":
                return EnchantmentTarget.VANISHABLE;
            default:
                return EnchantmentTarget.ALL;
        }
    }

    @Override
    public boolean isTreasure() {
        return inTag("treasure");
    }

    @Override
    public boolean isCursed() {
        return inTag("curse");
    }

    @Override
    public boolean conflictsWith(Enchantment other) {
        return other != null && !other.equals(this) && exclusiveIds().contains(other.getKey().toString());
    }

    @Override
    public boolean canEnchantItem(ItemStack item) {
        return item != null && supportedIds().contains(item.getType().getKey().toString());
    }

    @Override
    public Component displayName(int level) {
        Component name = description().color(isCursed() ? NamedTextColor.RED : NamedTextColor.GRAY);
        if (level != 1 || getMaxLevel() != 1) {
            name = name.append(Component.space()).append(Component.translatable("enchantment.level." + level));
        }
        return name;
    }

    @Override
    public boolean isTradeable() {
        return inTag("tradeable");
    }

    @Override
    public boolean isDiscoverable() {
        return inTag("non_treasure");
    }

    @Override
    public int getMinModifiedCost(int level) {
        Map<String, Object> cost = Nbt.map(definition().get("min_cost"));
        return Nbt.integer(cost, "base", 1) + Nbt.integer(cost, "per_level_above_first", 10) * (level - 1);
    }

    @Override
    public int getMaxModifiedCost(int level) {
        Map<String, Object> cost = Nbt.map(definition().get("max_cost"));
        return Nbt.integer(cost, "base", 1) + Nbt.integer(cost, "per_level_above_first", 10) * (level - 1);
    }

    @Override
    public int getAnvilCost() {
        return Nbt.integer(definition(), "anvil_cost", 1);
    }

    @Override
    public EnchantmentRarity getRarity() {
        int weight = getWeight();
        if (weight >= 10) {
            return EnchantmentRarity.COMMON;
        }
        if (weight >= 5) {
            return EnchantmentRarity.UNCOMMON;
        }
        return weight >= 2 ? EnchantmentRarity.RARE : EnchantmentRarity.VERY_RARE;
    }

    @Override
    public float getDamageIncrease(int level, EntityCategory category) {
        String name = key.getKey();
        if (name.equals("sharpness")) {
            return 1.0F + Math.max(0, level - 1) * 0.5F;
        }
        if (name.equals("smite") && category == EntityCategory.UNDEAD || name.equals("bane_of_arthropods") && category == EntityCategory.ARTHROPOD) {
            return level * 2.5F;
        }
        return 0.0F;
    }

    @Override
    public float getDamageIncrease(int level, EntityType type) {
        String name = key.getKey();
        if (name.equals("sharpness")) {
            return 1.0F + Math.max(0, level - 1) * 0.5F;
        }
        return 0.0F;
    }

    @Override
    public Set<EquipmentSlotGroup> getActiveSlotGroups() {
        Set<EquipmentSlotGroup> out = new LinkedHashSet<>();
        Object slots = definition().get("slots");
        if (slots instanceof List<?> list) {
            for (Object name : list) {
                EquipmentSlotGroup group = EquipmentSlotGroup.getByName(String.valueOf(name));
                if (group != null) {
                    out.add(group);
                }
            }
        }
        return out;
    }

    @Override
    public Component description() {
        Object description = definition().get("description");
        return description == null ? Component.translatable("enchantment." + key.getNamespace() + "." + key.getKey())
            : Texts.fromNbt(description);
    }

    @Override
    public RegistryKeySet<org.bukkit.inventory.ItemType> getSupportedItems() {
        return new KeySet<>(RegistryKey.ITEM, supportedIds());
    }

    @Override
    public RegistryKeySet<org.bukkit.inventory.ItemType> getPrimaryItems() {
        Object spec = definition().get("primary_items");
        return spec == null ? null : new KeySet<>(RegistryKey.ITEM, TagSets.resolve("items", spec));
    }

    @Override
    public int getWeight() {
        return Nbt.integer(definition(), "weight", 1);
    }

    @Override
    public RegistryKeySet<Enchantment> getExclusiveWith() {
        return new KeySet<>(RegistryKey.ENCHANTMENT, exclusiveIds());
    }

    @Override
    public String translationKey() {
        return "enchantment." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public String getTranslationKey() {
        return translationKey();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltEnchantment enchantment && enchantment.key.equals(key);
    }

    @Override
    public int hashCode() {
        return key.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltEnchantment[" + key + "]";
    }
}
