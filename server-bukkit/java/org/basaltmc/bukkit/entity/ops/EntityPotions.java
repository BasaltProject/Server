package org.basaltmc.bukkit.entity.ops;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.entity.EntityCore;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Color;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.potion.PotionData;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionType;

/** The potion contents an entity keeps (an area effect cloud, an arrow), read and written as one compound. */
final class EntityPotions {
    private final EntityCore core;
    private final String path;

    EntityPotions(EntityCore core, String path) {
        this.core = core;
        this.path = path;
    }

    private Map<String, Object> read() {
        Object stored = EntityNbt.compound(core, "").get(path);
        Map<String, Object> contents = Nbt.map(stored);
        return contents == null ? Nbt.compound() : Nbt.deepCopy(contents);
    }

    private void write(Map<String, Object> contents) {
        if (contents.isEmpty()) {
            core.nbtRemove(path);
        } else {
            EntityNbt.put(core, path, contents);
        }
    }

    PotionType type() {
        Object id = read().get("potion");
        return id == null ? null : Registry.POTION.get(NamespacedKey.fromString(String.valueOf(id)));
    }

    void type(PotionType type) {
        Map<String, Object> contents = read();
        if (type == null) {
            contents.remove("potion");
        } else {
            contents.put("potion", type.getKey().toString());
        }
        write(contents);
        if (type == null) {
            core.nbtRemove(path + ".potion");
        }
    }

    static PotionType of(PotionData data) {
        String base = data.getType().name();
        String variant = data.isExtended() ? "LONG_" + base : data.isUpgraded() ? "STRONG_" + base : base;
        try {
            return PotionType.valueOf(variant);
        } catch (IllegalArgumentException missing) {
            return data.getType();
        }
    }

    static PotionData dataOf(PotionType type) {
        if (type == null) {
            return new PotionData(PotionType.WATER);
        }
        String name = type.name();
        if (name.startsWith("LONG_")) {
            return new PotionData(PotionType.valueOf(name.substring(5)), false, true);
        } else if (name.startsWith("STRONG_")) {
            return new PotionData(PotionType.valueOf(name.substring(7)), true, false);
        }
        return new PotionData(type);
    }

    Color color() {
        Object stored = read().get("custom_color");
        if (stored instanceof Number number) {
            return Color.fromRGB(number.intValue() & 0xFFFFFF);
        }
        return null;
    }

    void color(Color color) {
        Map<String, Object> contents = read();
        if (color == null) {
            contents.remove("custom_color");
            write(contents);
            core.nbtRemove(path + ".custom_color");
        } else {
            contents.put("custom_color", color.asRGB());
            write(contents);
        }
    }

    List<PotionEffect> effects() {
        return Convert.effects(read().get("custom_effects"));
    }

    boolean add(PotionEffect effect, boolean override) {
        List<PotionEffect> effects = new ArrayList<>(effects());
        for (PotionEffect held : effects) {
            if (held.getType().equals(effect.getType())) {
                if (!override) {
                    return false;
                }
                effects.remove(held);
                break;
            }
        }
        effects.add(effect);
        store(effects);
        return true;
    }

    boolean remove(PotionEffectType type) {
        List<PotionEffect> effects = new ArrayList<>(effects());
        boolean removed = effects.removeIf(effect -> effect.getType().equals(type));
        if (removed) {
            store(effects);
        }
        return removed;
    }

    boolean has(PotionEffectType type) {
        return effects().stream().anyMatch(effect -> effect.getType().equals(type));
    }

    void clear() {
        store(List.of());
    }

    private void store(List<PotionEffect> effects) {
        Map<String, Object> contents = read();
        List<Object> list = new ArrayList<>();
        for (PotionEffect effect : effects) {
            list.add(Convert.effect(effect));
        }
        contents.put("custom_effects", list);
        write(contents);
    }
}
