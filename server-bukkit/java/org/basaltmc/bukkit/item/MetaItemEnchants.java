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

public abstract class MetaItemEnchants extends MetaItemComponents {
    protected MetaItemEnchants(Material type, Map<String, Object> components) {
        super(type, components);
    }

    protected Map<String, Object> enchantmentMap(String component, boolean create) {
        Map<String, Object> found = compoundOf(component);
        if (found == null && create) {
            found = Nbt.compound();
            c.put(component, found);
        }
        return found;
    }

    protected final boolean hasEnchantsIn(String component) {
        Map<String, Object> found = compoundOf(component);
        return found != null && !found.isEmpty();
    }

    protected final Map<Enchantment, Integer> enchantsIn(String component) {
        Map<String, Object> found = compoundOf(component);
        Map<Enchantment, Integer> out = new LinkedHashMap<>();
        if (found != null) {
            for (Map.Entry<String, Object> entry : found.entrySet()) {
                NamespacedKey key = NamespacedKey.fromString(entry.getKey());
                Enchantment enchantment = key == null ? null : Enchantment.getByKey(key);
                if (enchantment != null && entry.getValue() instanceof Number level) {
                    out.put(enchantment, level.intValue());
                }
            }
        }
        return out;
    }

    protected final boolean addEnchantTo(String component, Enchantment enchantment, int level, boolean ignore) {
        if (enchantment == null) {
            throw new IllegalArgumentException("Enchantment cannot be null");
        }
        if (!ignore && (level < enchantment.getStartLevel() || level > enchantment.getMaxLevel())) {
            return false;
        }
        Map<String, Object> map = enchantmentMap(component, true);
        Object old = map.put(enchantment.getKey().toString(), level);
        return old == null || ((Number) old).intValue() != level;
    }

    protected final boolean removeEnchantFrom(String component, Enchantment enchantment) {
        Map<String, Object> map = compoundOf(component);
        if (map == null || map.remove(enchantment.getKey().toString()) == null) {
            return false;
        }
        if (map.isEmpty()) {
            c.remove(component);
        }
        return true;
    }

    @Override
    public boolean hasEnchants() {
        return hasEnchantsIn(kEnchantments);
    }

    @Override
    public boolean hasEnchant(Enchantment enchant) {
        Map<String, Object> map = compoundOf(kEnchantments);
        return map != null && enchant != null && map.containsKey(enchant.getKey().toString());
    }

    @Override
    public int getEnchantLevel(Enchantment enchant) {
        Map<String, Object> map = compoundOf(kEnchantments);
        return map == null || enchant == null ? 0 : Nbt.integer(map, enchant.getKey().toString(), 0);
    }

    @Override
    public Map<Enchantment, Integer> getEnchants() {
        return enchantsIn(kEnchantments);
    }

    @Override
    public boolean addEnchant(Enchantment enchant, int level, boolean ignoreLevelRestriction) {
        return addEnchantTo(kEnchantments, enchant, level, ignoreLevelRestriction);
    }

    @Override
    public boolean removeEnchant(Enchantment enchant) {
        return enchant != null && removeEnchantFrom(kEnchantments, enchant);
    }

    @Override
    public void removeEnchantments() {
        c.remove(kEnchantments);
    }

    @Override
    public boolean hasConflictingEnchant(Enchantment enchant) {
        if (enchant == null) {
            return false;
        }
        for (Enchantment existing : getEnchants().keySet()) {
            if (existing.conflictsWith(enchant)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void addItemFlags(ItemFlag... flags) {
        MetaFlags.add(c, flags);
    }

    @Override
    public void removeItemFlags(ItemFlag... flags) {
        MetaFlags.remove(c, flags);
    }

    @Override
    public Set<ItemFlag> getItemFlags() {
        return MetaFlags.all(c);
    }

    @Override
    public boolean hasItemFlag(ItemFlag flag) {
        return MetaFlags.has(c, flag);
    }

    @Override
    public boolean isHideTooltip() {
        return Nbt.flag(compoundOf(kTooltipDisplay), "hide_tooltip", false);
    }

    @Override
    public void setHideTooltip(boolean hide) {
        Map<String, Object> display = compoundOf(kTooltipDisplay);
        if (!hide) {
            if (display != null) {
                display.remove("hide_tooltip");
                if (display.isEmpty()) {
                    c.remove(kTooltipDisplay);
                }
            }
            return;
        }
        if (display == null) {
            display = Nbt.compound();
            c.put(kTooltipDisplay, display);
        }
        display.put("hide_tooltip", Nbt.bool(true));
    }

    @Override
    public boolean hasTooltipStyle() {
        return has("minecraft:tooltip_style");
    }

    @Override
    public NamespacedKey getTooltipStyle() {
        return key("minecraft:tooltip_style");
    }

    @Override
    public void setTooltipStyle(NamespacedKey style) {
        put("minecraft:tooltip_style", style == null ? null : style.toString());
    }

    @Override
    public boolean hasItemModel() {
        return has("minecraft:item_model");
    }

    @Override
    public NamespacedKey getItemModel() {
        return key("minecraft:item_model");
    }

    @Override
    public void setItemModel(NamespacedKey model) {
        put("minecraft:item_model", model == null ? null : model.toString());
    }
}
