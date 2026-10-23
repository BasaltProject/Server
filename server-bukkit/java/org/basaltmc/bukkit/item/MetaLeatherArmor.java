package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.Color;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.ColorableArmorMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaLeatherArmor extends MetaArmor implements ColorableArmorMeta {
    static final Color kDefault = Color.fromRGB(0xA06540);

    public MetaLeatherArmor(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaLeatherArmor clone() {
        return (MetaLeatherArmor) super.clone();
    }

    @Override
    public Color getColor() {
        return c.get("minecraft:dyed_color") instanceof Number rgb ? Color.fromRGB(rgb.intValue() & 0xFFFFFF) : kDefault;
    }

    @Override
    public void setColor(Color color) {
        put("minecraft:dyed_color", color == null ? null : (Object) color.asRGB());
    }

    @Override
    public boolean isDyed() {
        return c.containsKey("minecraft:dyed_color");
    }
}
