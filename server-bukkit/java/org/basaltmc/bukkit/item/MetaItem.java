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

/** Item meta over the vanilla data components of one stack; every getter and setter reads or writes the map. */
@org.bukkit.configuration.serialization.DelegateDeserialization(ItemMetas.Serial.class)
public class MetaItem extends ItemMetaBase implements Damageable, Repairable,
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

    public MetaItem(Material type, Map<String, Object> components) {
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

    @Override
    public boolean hasCustomModelData() {
        return !modelFloats().isEmpty();
    }

    private List<Object> modelFloats() {
        Map<String, Object> data = compoundOf(kModelData);
        List<Object> floats = data == null ? null : Nbt.list(data.get("floats"));
        return floats == null ? List.of() : floats;
    }

    @Override
    public int getCustomModelData() {
        List<Object> floats = modelFloats();
        if (floats.isEmpty()) {
            throw new IllegalStateException("We don't have CustomModelData! Check hasCustomModelData first!");
        }
        return ((Number) floats.get(0)).intValue();
    }

    @Override
    public void setCustomModelData(Integer data) {
        if (data == null) {
            c.remove(kModelData);
            return;
        }
        Map<String, Object> model = Nbt.compound();
        List<Object> floats = new ArrayList<>();
        floats.add(data.floatValue());
        model.put("floats", floats);
        c.put(kModelData, model);
    }

    @Override
    public boolean hasCustomModelDataComponent() {
        return has(kModelData);
    }

    @Override
    public CustomModelDataComponent getCustomModelDataComponent() {
        return new MetaModelData.ModelData(Nbt.deepCopy(has(kModelData) ? compoundOf(kModelData) : Nbt.compound()));
    }

    @Override
    public void setCustomModelDataComponent(CustomModelDataComponent component) {
        put(kModelData, component == null ? null : ((MetaModelData.ModelData) component).data);
    }

    @Override
    public boolean hasBlockData() {
        return has("minecraft:block_state");
    }

    @Override
    public org.bukkit.block.data.BlockData getBlockData(Material material) {
        if (!material.isBlock()) {
            throw new IllegalArgumentException("Material " + material + " is not a block");
        }
        org.bukkit.block.data.BlockData base = material.createBlockData();
        Map<String, Object> stored = compoundOf("minecraft:block_state");
        if (stored == null || stored.isEmpty()) {
            return base;
        }
        StringBuilder properties = new StringBuilder("[");
        for (Map.Entry<String, Object> entry : stored.entrySet()) {
            properties.append(properties.length() > 1 ? "," : "").append(entry.getKey()).append('=')
                .append(entry.getValue());
        }
        return base.merge(org.bukkit.Bukkit.createBlockData(material, properties.append(']').toString()));
    }

    @Override
    public void setBlockData(org.bukkit.block.data.BlockData data) {
        if (data == null) {
            c.remove("minecraft:block_state");
            return;
        }
        String text = data.getAsString();
        int open = text.indexOf('[');
        Map<String, Object> stored = Nbt.compound();
        if (open >= 0) {
            for (String pair : text.substring(open + 1, text.length() - 1).split(",")) {
                String[] parts = pair.split("=", 2);
                stored.put(parts[0], parts[1]);
            }
        }
        put("minecraft:block_state", stored);
    }

    @Override
    public boolean hasFood() {
        return has("minecraft:food");
    }

    @Override
    public org.bukkit.inventory.meta.components.FoodComponent getFood() {
        return new MetaComponents.Food(has("minecraft:food") ? Nbt.deepCopy(compoundOf("minecraft:food"))
            : Nbt.compound());
    }

    @Override
    public void setFood(org.bukkit.inventory.meta.components.FoodComponent food) {
        put("minecraft:food", food == null ? null : Nbt.deepCopy(((MetaComponents.Food) food).data));
    }

    @Override
    public boolean hasTool() {
        return has("minecraft:tool");
    }

    @Override
    public org.bukkit.inventory.meta.components.ToolComponent getTool() {
        return new MetaComponents.Tool(has("minecraft:tool") ? Nbt.deepCopy(compoundOf("minecraft:tool"))
            : Nbt.compound());
    }

    @Override
    public void setTool(org.bukkit.inventory.meta.components.ToolComponent tool) {
        put("minecraft:tool", tool == null ? null : Nbt.deepCopy(((MetaComponents.Tool) tool).data));
    }

    @Override
    public boolean hasEquippable() {
        return has("minecraft:equippable");
    }

    @Override
    public org.bukkit.inventory.meta.components.EquippableComponent getEquippable() {
        return has("minecraft:equippable") ? new MetaComponents.Equippable(Nbt.deepCopy(compoundOf("minecraft:equippable")))
            : MetaComponents.Equippable.empty();
    }

    @Override
    public void setEquippable(org.bukkit.inventory.meta.components.EquippableComponent equippable) {
        put("minecraft:equippable", equippable == null ? null
            : Nbt.deepCopy(((MetaComponents.Equippable) equippable).data));
    }

    @Override
    public boolean hasUseCooldown() {
        return has("minecraft:use_cooldown");
    }

    @Override
    public org.bukkit.inventory.meta.components.UseCooldownComponent getUseCooldown() {
        return has("minecraft:use_cooldown") ? new MetaComponents.Cooldown(Nbt.deepCopy(compoundOf("minecraft:use_cooldown")))
            : MetaComponents.Cooldown.empty();
    }

    @Override
    public void setUseCooldown(org.bukkit.inventory.meta.components.UseCooldownComponent cooldown) {
        put("minecraft:use_cooldown", cooldown == null ? null
            : Nbt.deepCopy(((MetaComponents.Cooldown) cooldown).data));
    }

    @Override
    public boolean hasJukeboxPlayable() {
        return has("minecraft:jukebox_playable");
    }

    @Override
    public org.bukkit.inventory.meta.components.JukeboxPlayableComponent getJukeboxPlayable() {
        return has("minecraft:jukebox_playable") ? MetaComponents.Jukebox.of(c.get("minecraft:jukebox_playable"))
            : MetaComponents.Jukebox.empty();
    }

    @Override
    public void setJukeboxPlayable(org.bukkit.inventory.meta.components.JukeboxPlayableComponent jukebox) {
        put("minecraft:jukebox_playable", jukebox == null ? null : ((MetaComponents.Jukebox) jukebox).stored());
    }

    @Override
    public boolean hasUseRemainder() {
        return has("minecraft:use_remainder");
    }

    @Override
    public org.bukkit.inventory.ItemStack getUseRemainder() {
        Map<String, Object> stored = compoundOf("minecraft:use_remainder");
        return stored == null ? null : Convert.stack(stored);
    }

    @Override
    public void setUseRemainder(org.bukkit.inventory.ItemStack remainder) {
        put("minecraft:use_remainder", remainder == null || remainder.isEmpty() ? null : Convert.stack(remainder));
    }

    @Override
    public io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.damage.DamageType> getDamageResistantTypes() {
        return has("minecraft:damage_resistant")
            ? org.basaltmc.bukkit.datacomponent.Builders.damageResistantOf(c.get("minecraft:damage_resistant")).types()
            : null;
    }

    @Override
    public void setDamageResistantTypes(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.damage.DamageType> types) {
        put("minecraft:damage_resistant", types == null ? null
            : org.basaltmc.bukkit.datacomponent.Builders.damageResistantNbt(types));
    }

    @Override
    public void setDamageResistant(org.bukkit.Tag<org.bukkit.damage.DamageType> tag) {
        if (tag == null) {
            c.remove("minecraft:damage_resistant");
            return;
        }
        Map<String, Object> resistant = Nbt.compound();
        resistant.put("types", "#" + tag.getKey());
        c.put("minecraft:damage_resistant", resistant);
    }

    @Override
    public boolean hasDestroyableKeys() {
        return has("minecraft:can_break");
    }

    @Override
    public java.util.Set<com.destroystokyo.paper.Namespaced> getDestroyableKeys() {
        return MetaFlags.keys(c, "minecraft:can_break");
    }

    @Override
    public void setDestroyableKeys(Collection<com.destroystokyo.paper.Namespaced> keys) {
        MetaFlags.setKeys(c, "minecraft:can_break", keys);
    }

    @Override
    public boolean hasPlaceableKeys() {
        return has("minecraft:can_place_on");
    }

    @Override
    public java.util.Set<com.destroystokyo.paper.Namespaced> getPlaceableKeys() {
        return MetaFlags.keys(c, "minecraft:can_place_on");
    }

    @Override
    public void setPlaceableKeys(Collection<com.destroystokyo.paper.Namespaced> keys) {
        MetaFlags.setKeys(c, "minecraft:can_place_on", keys);
    }

    @Override
    @SuppressWarnings("deprecation")
    public org.bukkit.inventory.meta.tags.CustomItemTagContainer getCustomTagContainer() {
        return new LegacyTags(getPersistentDataContainer());
    }

    @Override
    public boolean hasEnchantable() {
        return has("minecraft:enchantable");
    }

    @Override
    public int getEnchantable() {
        if (!hasEnchantable()) {
            throw new IllegalStateException("'enchantable' component is not set! Check hasEnchantable first!");
        }
        return Nbt.integer(compoundOf("minecraft:enchantable"), "value", 0);
    }

    @Override
    public void setEnchantable(Integer enchantable) {
        if (enchantable == null) {
            c.remove("minecraft:enchantable");
            return;
        }
        if (enchantable <= 0) {
            throw new IllegalArgumentException("Enchantable must be positive");
        }
        Map<String, Object> value = Nbt.compound();
        value.put("value", enchantable);
        c.put("minecraft:enchantable", value);
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

    protected final NamespacedKey key(String component) {
        return c.get(component) instanceof String text ? NamespacedKey.fromString(text) : null;
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
