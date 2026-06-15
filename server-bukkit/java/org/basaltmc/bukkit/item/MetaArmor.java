package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.ArmorMeta;
import org.bukkit.inventory.meta.trim.ArmorTrim;

@DelegateDeserialization(ItemMetas.Serial.class)
public class MetaArmor extends MetaItem implements ArmorMeta {
    public MetaArmor(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaArmor clone() {
        return (MetaArmor) super.clone();
    }

    @Override
    public boolean hasTrim() {
        return c.get("minecraft:trim") instanceof Map;
    }

    @Override
    public void setTrim(ArmorTrim trim) {
        if (trim == null) {
            c.remove("minecraft:trim");
            return;
        }
        Map<String, Object> out = Nbt.compound();
        out.put("material", trim.getMaterial().getKey().toString());
        out.put("pattern", trim.getPattern().getKey().toString());
        c.put("minecraft:trim", out);
    }

    @Override
    public ArmorTrim getTrim() {
        Map<String, Object> trim = compoundOf("minecraft:trim");
        if (trim == null) {
            return null;
        }
        NamespacedKey material = NamespacedKey.fromString(Nbt.string(trim, "material", ""));
        NamespacedKey pattern = NamespacedKey.fromString(Nbt.string(trim, "pattern", ""));
        if (material == null || pattern == null) {
            return null;
        }
        return new ArmorTrim(Registry.TRIM_MATERIAL.getOrThrow(material), Registry.TRIM_PATTERN.getOrThrow(pattern));
    }
}
