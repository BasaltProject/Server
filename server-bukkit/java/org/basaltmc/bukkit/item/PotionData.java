package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.InventoryNatives;
import org.bukkit.NamespacedKey;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionType;

/** What one potion type does, taken from the server's potion registry. */
public final class PotionData implements PotionType.InternalPotionData {
    private static Map<String, List<Map<String, Object>>> potions;

    private final NamespacedKey key;

    private PotionData(NamespacedKey key) {
        this.key = key;
    }

    public static PotionData of(NamespacedKey key) {
        return new PotionData(key);
    }

    private static synchronized Map<String, List<Map<String, Object>>> table() {
        if (potions == null) {
            potions = new HashMap<>();
            byte[] bytes = InventoryNatives.potions();
            Map<String, Object> root = bytes == null ? null : Nbt.read(bytes);
            List<Object> entries = root == null ? null : Nbt.list(root.get("potions"));
            if (entries != null) {
                for (Object entry : entries) {
                    Map<String, Object> data = Nbt.map(entry);
                    List<Map<String, Object>> effects = new ArrayList<>();
                    List<Object> listed = Nbt.list(data.get("effects"));
                    if (listed != null) {
                        for (Object effect : listed) {
                            effects.add(Nbt.map(effect));
                        }
                    }
                    potions.put(Nbt.string(data, "id", ""), effects);
                }
            }
        }
        return potions;
    }

    private List<Map<String, Object>> own() {
        List<Map<String, Object>> found = table().get(key.toString());
        return found == null ? List.of() : found;
    }

    @Override
    public PotionEffectType getEffectType() {
        List<Map<String, Object>> effects = own();
        if (effects.isEmpty()) {
            return null;
        }
        NamespacedKey effect = NamespacedKey.fromString(Nbt.string(effects.get(0), "id", ""));
        return effect == null ? null : PotionEffectType.getByKey(effect);
    }

    @Override
    public List<PotionEffect> getPotionEffects() {
        List<PotionEffect> out = new ArrayList<>();
        for (Map<String, Object> effect : own()) {
            NamespacedKey id = NamespacedKey.fromString(Nbt.string(effect, "id", ""));
            PotionEffectType type = id == null ? null : PotionEffectType.getByKey(id);
            if (type != null) {
                out.add(new PotionEffect(type, Nbt.integer(effect, "duration", 0), Nbt.integer(effect, "amplifier", 0)));
            }
        }
        return out;
    }

    @Override
    public boolean isInstant() {
        PotionEffectType type = getEffectType();
        return type != null && type.isInstant();
    }

    @Override
    public boolean isUpgradeable() {
        return table().containsKey(key.getNamespace() + ":strong_" + key.getKey());
    }

    @Override
    public boolean isExtendable() {
        return table().containsKey(key.getNamespace() + ":long_" + key.getKey());
    }

    @Override
    public int getMaxLevel() {
        return isUpgradeable() ? 2 : 1;
    }
}
