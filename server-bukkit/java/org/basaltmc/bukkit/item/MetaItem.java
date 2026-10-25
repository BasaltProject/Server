package org.basaltmc.bukkit.item;

import com.google.common.collect.Multimap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.gen.ItemMetaBase;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeModifier;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemFlag;
import org.bukkit.inventory.ItemRarity;
import org.bukkit.inventory.meta.Damageable;
import org.bukkit.inventory.meta.Repairable;
import org.bukkit.inventory.meta.components.CustomModelDataComponent;
import org.bukkit.persistence.PersistentDataContainer;

@org.bukkit.configuration.serialization.DelegateDeserialization(ItemMetas.Serial.class)
public class MetaItem extends MetaItemEnchants {
    public MetaItem(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public boolean isUnbreakable() {
        return has(kUnbreakable);
    }

    @Override
    public void setUnbreakable(boolean unbreakable) {
        if (unbreakable) {
            c.put(kUnbreakable, Nbt.compound());
        } else {
            c.remove(kUnbreakable);
        }
    }

    @Override
    public boolean hasEnchantmentGlintOverride() {
        return has("minecraft:enchantment_glint_override");
    }

    @Override
    public Boolean getEnchantmentGlintOverride() {
        if (!hasEnchantmentGlintOverride()) {
            throw new IllegalStateException("'enchantment_glint_override' component is not set! Check hasEnchantmentGlintOverride first!");
        }
        return Nbt.flag(c, "minecraft:enchantment_glint_override", false);
    }

    @Override
    public void setEnchantmentGlintOverride(Boolean override) {
        put("minecraft:enchantment_glint_override", override == null ? null : Nbt.bool(override));
    }

    @Override
    public boolean isGlider() {
        return has("minecraft:glider");
    }

    @Override
    public void setGlider(boolean glider) {
        if (glider) {
            c.put("minecraft:glider", Nbt.compound());
        } else {
            c.remove("minecraft:glider");
        }
    }

    @Override
    public boolean isFireResistant() {
        Map<String, Object> resistant = compoundOf("minecraft:damage_resistant");
        return resistant != null && "#minecraft:is_fire".equals(resistant.get("types"));
    }

    @Override
    public void setFireResistant(boolean fireResistant) {
        if (fireResistant) {
            Map<String, Object> resistant = Nbt.compound();
            resistant.put("types", "#minecraft:is_fire");
            c.put("minecraft:damage_resistant", resistant);
        } else if (isFireResistant()) {
            c.remove("minecraft:damage_resistant");
        }
    }

    @Override
    public org.bukkit.Tag<org.bukkit.damage.DamageType> getDamageResistant() {
        Map<String, Object> resistant = compoundOf("minecraft:damage_resistant");
        Object types = resistant == null ? null : resistant.get("types");
        if (!(types instanceof String text) || !text.startsWith("#")) {
            return null;
        }
        return org.bukkit.Bukkit.getTag("damage_type", org.bukkit.NamespacedKey.fromString(text.substring(1)),
            org.bukkit.damage.DamageType.class);
    }

    @Override
    public boolean hasDamageResistant() {
        return has("minecraft:damage_resistant");
    }

    @Override
    public boolean hasMaxStackSize() {
        return has("minecraft:max_stack_size");
    }

    @Override
    public int getMaxStackSize() {
        if (!hasMaxStackSize()) {
            throw new IllegalStateException("'max_stack_size' component is not set! Check hasMaxStackSize first!");
        }
        return Nbt.integer(c, "minecraft:max_stack_size", 64);
    }

    @Override
    public void setMaxStackSize(Integer max) {
        if (max != null && (max < 1 || max > 99)) {
            throw new IllegalArgumentException("max_stack_size must be between 1 and 99");
        }
        put("minecraft:max_stack_size", max);
    }

    @Override
    public boolean hasRarity() {
        return has("minecraft:rarity");
    }

    @Override
    public ItemRarity getRarity() {
        if (!hasRarity()) {
            throw new IllegalStateException("'rarity' component is not set! Check hasRarity first!");
        }
        return ItemRarity.valueOf(Nbt.string(c, "minecraft:rarity", "common").toUpperCase(java.util.Locale.ROOT));
    }

    @Override
    public void setRarity(ItemRarity rarity) {
        put("minecraft:rarity", rarity == null ? null : rarity.name().toLowerCase(java.util.Locale.ROOT));
    }

    @Override
    public boolean hasAttributeModifiers() {
        return MetaAttributes.has(c);
    }

    @Override
    public Multimap<Attribute, AttributeModifier> getAttributeModifiers() {
        return hasAttributeModifiers() ? MetaAttributes.all(c, entry -> true) : null;
    }

    @Override
    public Multimap<Attribute, AttributeModifier> getAttributeModifiers(EquipmentSlot slot) {
        return MetaAttributes.forSlot(c, slot);
    }

    @Override
    public Collection<AttributeModifier> getAttributeModifiers(Attribute attribute) {
        return MetaAttributes.forAttribute(c, attribute);
    }

    @Override
    public boolean addAttributeModifier(Attribute attribute, AttributeModifier modifier) {
        return MetaAttributes.add(c, attribute, modifier);
    }

    @Override
    public void setAttributeModifiers(Multimap<Attribute, AttributeModifier> modifiers) {
        MetaAttributes.set(c, modifiers);
    }

    @Override
    public boolean removeAttributeModifier(Attribute attribute) {
        return MetaAttributes.removeAttribute(c, attribute);
    }

    @Override
    public boolean removeAttributeModifier(EquipmentSlot slot) {
        return MetaAttributes.removeSlot(c, slot);
    }

    @Override
    public boolean removeAttributeModifier(Attribute attribute, AttributeModifier modifier) {
        return MetaAttributes.remove(c, attribute, modifier);
    }

    @Override
    public boolean hasDamage() {
        return Nbt.integer(c, kDamage, 0) > 0;
    }

    @Override
    public int getDamage() {
        return Nbt.integer(c, kDamage, 0);
    }

    @Override
    public void setDamage(int damage) {
        put(kDamage, damage <= 0 ? null : (Object) damage);
    }

    @Override
    public boolean hasDamageValue() {
        return has(kDamage);
    }

    @Override
    public void resetDamage() {
        c.remove(kDamage);
    }

    @Override
    public boolean hasMaxDamage() {
        return has(kMaxDamage);
    }

    @Override
    public int getMaxDamage() {
        if (!hasMaxDamage()) {
            throw new IllegalStateException("'max_damage' component is not set! Check hasMaxDamage first!");
        }
        return Nbt.integer(c, kMaxDamage, 0);
    }

    @Override
    public void setMaxDamage(Integer maxDamage) {
        if (maxDamage != null && maxDamage <= 0) {
            throw new IllegalArgumentException("Max damage must be positive");
        }
        put(kMaxDamage, maxDamage);
    }

    @Override
    public boolean hasRepairCost() {
        return Nbt.integer(c, kRepairCost, 0) > 0;
    }

    @Override
    public int getRepairCost() {
        return Nbt.integer(c, kRepairCost, 0);
    }

    @Override
    public void setRepairCost(int cost) {
        put(kRepairCost, cost <= 0 ? null : (Object) cost);
    }

    @Override
    public PersistentDataContainer getPersistentDataContainer() {
        return new ItemPdc(c);
    }

    @Override
    public String getAsString() {
        return ItemMetas.snbt(c);
    }

    @Override
    public String getAsComponentString() {
        return ItemMetas.snbt(c);
    }

    @Override
    public Set<Material> getCanDestroy() {
        return MetaFlags.blocks(c, "minecraft:can_break");
    }

    @Override
    public void setCanDestroy(Set<Material> blocks) {
        MetaFlags.setBlocks(c, "minecraft:can_break", blocks);
    }

    @Override
    public Set<Material> getCanPlaceOn() {
        return MetaFlags.blocks(c, "minecraft:can_place_on");
    }

    @Override
    public void setCanPlaceOn(Set<Material> blocks) {
        MetaFlags.setBlocks(c, "minecraft:can_place_on", blocks);
    }

    @Override
    public void setVersion(int version) {}

    @Override
    public Map<String, Object> serialize() {
        return ItemMetas.serialize(this);
    }

    @Override
    public String toString() {
        return "ItemMeta" + ItemMetas.snbt(c);
    }
}
