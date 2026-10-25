package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import io.papermc.paper.potion.SuspiciousEffectEntry;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.BundleMeta;
import org.bukkit.inventory.meta.CrossbowMeta;
import org.bukkit.inventory.meta.SuspiciousStewMeta;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

public final class MetaContainers {
    private MetaContainers() {}

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Bundles extends MetaItem implements BundleMeta {
        private static final String kContents = "minecraft:bundle_contents";

        public Bundles(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Bundles clone() {
            return (Bundles) super.clone();
        }

        @Override
        public boolean hasItems() {
            List<Object> list = Nbt.list(c.get(kContents));
            return list != null && !list.isEmpty();
        }

        @Override
        public List<ItemStack> getItems() {
            return Convert.stacksOf(c.get(kContents));
        }

        @Override
        public void setItems(List<ItemStack> items) {
            if (items == null || items.isEmpty()) {
                c.remove(kContents);
            } else {
                c.put(kContents, Convert.stacks(items));
            }
        }

        @Override
        public void addItem(ItemStack item) {
            if (item == null || item.getType().isAir()) {
                throw new IllegalArgumentException("Item cannot be null or empty");
            }
            List<ItemStack> items = getItems();
            items.add(item.clone());
            setItems(items);
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Crossbows extends MetaItem implements CrossbowMeta {
        private static final String kCharged = "minecraft:charged_projectiles";

        public Crossbows(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Crossbows clone() {
            return (Crossbows) super.clone();
        }

        @Override
        public boolean hasChargedProjectiles() {
            List<Object> list = Nbt.list(c.get(kCharged));
            return list != null && !list.isEmpty();
        }

        @Override
        public List<ItemStack> getChargedProjectiles() {
            return Convert.stacksOf(c.get(kCharged));
        }

        @Override
        public void setChargedProjectiles(List<ItemStack> projectiles) {
            if (projectiles == null || projectiles.isEmpty()) {
                c.remove(kCharged);
            } else {
                c.put(kCharged, Convert.stacks(projectiles));
            }
        }

        @Override
        public void addChargedProjectile(ItemStack item) {
            if (item == null) {
                throw new IllegalArgumentException("Item cannot be null");
            }
            List<ItemStack> items = getChargedProjectiles();
            items.add(item.clone());
            setChargedProjectiles(items);
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Stews extends MetaItem implements SuspiciousStewMeta {
        private static final String kEffects = "minecraft:suspicious_stew_effects";

        public Stews(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Stews clone() {
            return (Stews) super.clone();
        }

        private List<Object> raw() {
            List<Object> list = Nbt.list(c.get(kEffects));
            return list == null ? new ArrayList<>() : list;
        }

        @Override
        public boolean hasCustomEffects() {
            return !raw().isEmpty();
        }

        @Override
        public List<PotionEffect> getCustomEffects() {
            List<PotionEffect> out = new ArrayList<>();
            for (Object entry : raw()) {
                Map<String, Object> data = Nbt.map(entry);
                NamespacedKey key = data == null ? null : NamespacedKey.fromString(Nbt.string(data, "id", ""));
                PotionEffectType type = key == null ? null : PotionEffectType.getByKey(key);
                if (type != null) {
                    out.add(new PotionEffect(type, Nbt.integer(data, "duration", 160), 0));
                }
            }
            return out;
        }

        @Override
        public boolean addCustomEffect(PotionEffect effect, boolean overwrite) {
            if (effect == null) {
                throw new IllegalArgumentException("Potion effect cannot be null");
            }
            return add(effect.getType(), effect.getDuration(), overwrite);
        }

        @Override
        public boolean addCustomEffect(SuspiciousEffectEntry effect, boolean overwrite) {
            return add(effect.effect(), effect.duration(), overwrite);
        }

        private boolean add(PotionEffectType type, int duration, boolean overwrite) {
            List<Object> list = raw();
            String id = type.getKey().toString();
            for (int at = 0; at < list.size(); at++) {
                Map<String, Object> data = Nbt.map(list.get(at));
                if (data != null && id.equals(Nbt.string(data, "id", ""))) {
                    if (!overwrite || Nbt.integer(data, "duration", 160) == duration) {
                        return false;
                    }
                    data.put("duration", duration);
                    c.put(kEffects, list);
                    return true;
                }
            }
            Map<String, Object> entry = Nbt.compound();
            entry.put("id", id);
            entry.put("duration", duration);
            list.add(entry);
            c.put(kEffects, list);
            return true;
        }

        @Override
        public boolean removeCustomEffect(PotionEffectType type) {
            List<Object> list = raw();
            String id = type.getKey().toString();
            boolean changed = list.removeIf(entry -> entry instanceof Map && id.equals(Nbt.string(Nbt.map(entry), "id", "")));
            if (changed) {
                if (list.isEmpty()) {
                    c.remove(kEffects);
                } else {
                    c.put(kEffects, list);
                }
            }
            return changed;
        }

        @Override
        public boolean hasCustomEffect(PotionEffectType type) {
            String id = type.getKey().toString();
            for (Object entry : raw()) {
                if (entry instanceof Map && id.equals(Nbt.string(Nbt.map(entry), "id", ""))) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public boolean clearCustomEffects() {
            boolean had = c.containsKey(kEffects);
            c.remove(kEffects);
            return had;
        }
    }
}
