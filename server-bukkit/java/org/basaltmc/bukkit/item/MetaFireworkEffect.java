package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.FireworkEffect;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.FireworkEffectMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaFireworkEffect extends MetaItem implements FireworkEffectMeta {
    private static final String kExplosion = "minecraft:firework_explosion";

    public MetaFireworkEffect(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaFireworkEffect clone() {
        return (MetaFireworkEffect) super.clone();
    }

    @Override
    public void setEffect(FireworkEffect effect) {
        put(kExplosion, effect == null ? null : Convert.firework(effect));
    }

    @Override
    public boolean hasEffect() {
        return c.get(kExplosion) instanceof Map;
    }

    @Override
    public FireworkEffect getEffect() {
        Map<String, Object> found = compoundOf(kExplosion);
        return found == null ? null : Convert.firework(found);
    }
}
