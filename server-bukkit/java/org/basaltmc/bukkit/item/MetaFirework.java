package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.bukkit.FireworkEffect;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.FireworkMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaFirework extends MetaItem implements FireworkMeta {
    private static final String kFireworks = "minecraft:fireworks";

    public MetaFirework(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaFirework clone() {
        return (MetaFirework) super.clone();
    }

    private Map<String, Object> data(boolean create) {
        Map<String, Object> found = compoundOf(kFireworks);
        if (found == null && create) {
            found = Nbt.compound();
            c.put(kFireworks, found);
        }
        return found;
    }

    private void tidy() {
        Map<String, Object> found = compoundOf(kFireworks);
        if (found != null && Nbt.integer(found, "flight_duration", 0) == 0 && getEffectsSize() == 0) {
            c.remove(kFireworks);
        }
    }

    @Override
    public void addEffect(FireworkEffect effect) {
        if (effect == null) {
            throw new IllegalArgumentException("FireworkEffect cannot be null");
        }
        List<Object> list = explosions();
        list.add(Convert.firework(effect));
        data(true).put("explosions", list);
    }

    @Override
    public void addEffects(FireworkEffect... effects) {
        for (FireworkEffect effect : effects) {
            addEffect(effect);
        }
    }

    @Override
    public void addEffects(Iterable<FireworkEffect> effects) {
        for (FireworkEffect effect : effects) {
            addEffect(effect);
        }
    }

    private List<Object> explosions() {
        Map<String, Object> found = data(false);
        List<Object> list = found == null ? null : Nbt.list(found.get("explosions"));
        return list == null ? new ArrayList<>() : list;
    }

    @Override
    public List<FireworkEffect> getEffects() {
        List<FireworkEffect> out = new ArrayList<>();
        for (Object entry : explosions()) {
            Map<String, Object> map = Nbt.map(entry);
            if (map != null) {
                out.add(Convert.firework(map));
            }
        }
        return out;
    }

    @Override
    public int getEffectsSize() {
        return explosions().size();
    }

    @Override
    public void removeEffect(int index) {
        List<Object> list = explosions();
        list.remove(index);
        data(true).put("explosions", list);
        tidy();
    }

    @Override
    public void clearEffects() {
        Map<String, Object> found = data(false);
        if (found != null) {
            found.remove("explosions");
            tidy();
        }
    }

    @Override
    public boolean hasEffects() {
        return getEffectsSize() > 0;
    }

    @Override
    public boolean hasPower() {
        return getPower() > 0;
    }

    @Override
    public int getPower() {
        return Nbt.integer(data(false), "flight_duration", 0);
    }

    @Override
    public void setPower(int power) {
        if (power < 0 || power > 127) {
            throw new IllegalArgumentException("Power cannot be outside 0-127, given " + power);
        }
        data(true).put("flight_duration", (byte) power);
        tidy();
    }
}
