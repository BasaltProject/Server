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

/** Data components of an item meta. */
public abstract class MetaItemComponents extends MetaItemCore {
    protected MetaItemComponents(Material type, Map<String, Object> components) {
        super(type, components);
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
}
