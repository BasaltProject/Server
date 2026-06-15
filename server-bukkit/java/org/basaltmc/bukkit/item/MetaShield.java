package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.DyeColor;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.ShieldMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaShield extends MetaBanner implements ShieldMeta {
    public MetaShield(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaShield clone() {
        return (MetaShield) super.clone();
    }

    @Override
    public DyeColor getBaseColor() {
        return c.get("minecraft:base_color") instanceof String name ? Convert.dye(name) : null;
    }

    @Override
    public void setBaseColor(DyeColor color) {
        put("minecraft:base_color", color == null ? null : Convert.dye(color));
    }
}
