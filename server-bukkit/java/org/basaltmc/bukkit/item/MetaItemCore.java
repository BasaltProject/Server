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

/** Identity, name and lore of an item meta. */
public abstract class MetaItemCore extends ItemMetaBase implements Damageable, Repairable,
        org.bukkit.inventory.meta.BlockDataMeta {
    static final String kCustomName = "minecraft:custom_name";
    static final String kItemName = "minecraft:item_name";
    static final String kLore = "minecraft:lore";
    static final String kEnchantments = "minecraft:enchantments";
    static final String kStoredEnchantments = "minecraft:stored_enchantments";
    static final String kTooltipDisplay = "minecraft:tooltip_display";
    static final String kAttributes = "minecraft:attribute_modifiers";
    static final String kCustomData = "minecraft:custom_data";
    static final String kUnbreakable = "minecraft:unbreakable";
    static final String kDamage = "minecraft:damage";
    static final String kMaxDamage = "minecraft:max_damage";
    static final String kRepairCost = "minecraft:repair_cost";
    static final String kModelData = "minecraft:custom_model_data";

    protected Map<String, Object> c;
    protected final Material type;

    protected MetaItemCore(Material type, Map<String, Object> components) {
        this.type = type;
        this.c = components;
    }

    public final Map<String, Object> components() {
        return c;
    }

    public final Material materialType() {
        return type;
    }

    /** Whether a stack of this material may carry this meta; a plain meta fits anything. */
    public boolean applicableTo(Material material) {
        return ItemMetas.kindOf(material) == ItemMetas.kindOf(type) || ItemMetas.kindOf(type) == ItemMetas.Kind.PLAIN;
    }

    public boolean isEmptyMeta() {
        return c.isEmpty();
    }

    @Override
    public MetaItem clone() {
        return ItemMetas.create(type, Nbt.deepCopy(c));
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof MetaItem meta && ItemMetas.kindOf(meta.type) == ItemMetas.kindOf(type)
            && Nbt.same(c, meta.c);
    }

    @Override
    public int hashCode() {
        return Nbt.hash(c);
    }

    protected final NamespacedKey key(String component) {
        return c.get(component) instanceof String text ? NamespacedKey.fromString(text) : null;
    }

    protected final boolean has(String component) {
        return c.containsKey(component);
    }

    protected final void put(String component, Object value) {
        if (value == null) {
            c.remove(component);
        } else {
            c.put(component, value);
        }
    }

    protected final Map<String, Object> compoundOf(String component) {
        return Nbt.map(c.get(component));
    }

    @Override
    public boolean hasCustomName() {
        return has(kCustomName);
    }

    @Override
    public Component customName() {
        return Texts.fromNbt(c.get(kCustomName));
    }

    @Override
    public void customName(Component name) {
        put(kCustomName, name == null ? null : Texts.toNbt(name));
    }

    @Override
    public String getDisplayName() {
        String text = Texts.legacyFromNbt(c.get(kCustomName));
        return text == null ? "" : text;
    }

    @Override
    public void setDisplayName(String name) {
        put(kCustomName, name == null ? null : Texts.legacyToNbt(name));
    }

    @Override
    public net.md_5.bungee.api.chat.BaseComponent[] getDisplayNameComponent() {
        return has(kCustomName) ? bungee(customName()) : new net.md_5.bungee.api.chat.BaseComponent[0];
    }

    @Override
    public void setDisplayNameComponent(net.md_5.bungee.api.chat.BaseComponent[] component) {
        put(kCustomName, component == null ? null : Texts.toNbt(unbungee(component)));
    }

    static net.md_5.bungee.api.chat.BaseComponent[] bungee(Component component) {
        return net.md_5.bungee.chat.ComponentSerializer.parse(Texts.json(component));
    }

    static Component unbungee(net.md_5.bungee.api.chat.BaseComponent[] components) {
        return Texts.fromJson(net.md_5.bungee.chat.ComponentSerializer.toString(components));
    }

    @Override
    public boolean hasItemName() {
        return has(kItemName);
    }

    @Override
    public Component itemName() {
        return Texts.fromNbt(c.get(kItemName));
    }

    @Override
    public void itemName(Component name) {
        put(kItemName, name == null ? null : Texts.toNbt(name));
    }

    @Override
    public String getItemName() {
        String text = Texts.legacyFromNbt(c.get(kItemName));
        return text == null ? "" : text;
    }

    @Override
    public void setItemName(String name) {
        put(kItemName, name == null ? null : Texts.legacyToNbt(name));
    }

    @Override
    public boolean hasLocalizedName() {
        return false;
    }

    @Override
    public String getLocalizedName() {
        return getDisplayName();
    }

    @Override
    public void setLocalizedName(String name) {
        setDisplayName(name);
    }

    @Override
    public boolean hasLore() {
        List<Object> lore = Nbt.list(c.get(kLore));
        return lore != null && !lore.isEmpty();
    }

    @Override
    public List<Component> lore() {
        List<Object> lore = Nbt.list(c.get(kLore));
        if (lore == null) {
            return null;
        }
        List<Component> out = new ArrayList<>(lore.size());
        for (Object line : lore) {
            out.add(Texts.fromNbt(line));
        }
        return out;
    }

    @Override
    public void lore(List<? extends Component> lore) {
        if (lore == null || lore.isEmpty()) {
            c.remove(kLore);
            return;
        }
        List<Object> lines = new ArrayList<>(lore.size());
        boolean uniform = true;
        for (Component line : lore) {
            Object nbt = Texts.toNbt(line == null ? Component.empty() : line);
            lines.add(nbt);
            if ((nbt instanceof Map) != (lines.get(0) instanceof Map)) {
                uniform = false;
            }
        }
        if (!uniform) {
            for (int at = 0; at < lines.size(); at++) {
                if (!(lines.get(at) instanceof Map)) {
                    Map<String, Object> wrapped = new LinkedHashMap<>();
                    wrapped.put("", lines.get(at));
                    lines.set(at, wrapped);
                }
            }
        }
        c.put(kLore, lines);
    }

    @Override
    public List<String> getLore() {
        List<Component> lore = lore();
        if (lore == null) {
            return null;
        }
        List<String> out = new ArrayList<>(lore.size());
        for (Component line : lore) {
            out.add(Texts.toLegacy(line));
        }
        return out;
    }

    @Override
    public void setLore(List<String> lore) {
        if (lore == null) {
            c.remove(kLore);
            return;
        }
        List<Component> lines = new ArrayList<>(lore.size());
        for (String line : lore) {
            lines.add(Texts.fromLegacy(line));
        }
        lore(lines);
    }

    @Override
    public List<net.md_5.bungee.api.chat.BaseComponent[]> getLoreComponents() {
        List<Component> lore = lore();
        if (lore == null) {
            return null;
        }
        List<net.md_5.bungee.api.chat.BaseComponent[]> out = new ArrayList<>(lore.size());
        for (Component line : lore) {
            out.add(bungee(line));
        }
        return out;
    }

    @Override
    public void setLoreComponents(List<net.md_5.bungee.api.chat.BaseComponent[]> lore) {
        if (lore == null) {
            c.remove(kLore);
            return;
        }
        List<Component> lines = new ArrayList<>(lore.size());
        for (net.md_5.bungee.api.chat.BaseComponent[] line : lore) {
            lines.add(unbungee(line));
        }
        lore(lines);
    }
}
