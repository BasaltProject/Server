package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.bukkit.DyeColor;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.entity.Axolotl;
import org.bukkit.entity.TropicalFish;
import org.bukkit.inventory.meta.AxolotlBucketMeta;
import org.bukkit.inventory.meta.KnowledgeBookMeta;
import org.bukkit.inventory.meta.MusicInstrumentMeta;
import org.bukkit.inventory.meta.OminousBottleMeta;
import org.bukkit.inventory.meta.TropicalFishBucketMeta;

/** The metas that are one or two plain components. */
public final class MetaVariants {
    private MetaVariants() {}

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Axolotls extends MetaItem implements AxolotlBucketMeta {
        public Axolotls(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Axolotls clone() {
            return (Axolotls) super.clone();
        }

        @Override
        public boolean hasVariant() {
            return c.containsKey("minecraft:axolotl/variant");
        }

        @Override
        public Axolotl.Variant getVariant() {
            if (!hasVariant()) {
                throw new IllegalStateException("Variant is absent");
            }
            return Axolotl.Variant.valueOf(Nbt.string(c, "minecraft:axolotl/variant", "lucy").toUpperCase(Locale.ROOT));
        }

        @Override
        public void setVariant(Axolotl.Variant variant) {
            put("minecraft:axolotl/variant", variant == null ? null : variant.name().toLowerCase(Locale.ROOT));
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class TropicalFishes extends MetaItem implements TropicalFishBucketMeta {
        private static final String kPattern = "minecraft:tropical_fish/pattern";
        private static final String kBody = "minecraft:tropical_fish/base_color";
        private static final String kPatternColor = "minecraft:tropical_fish/pattern_color";

        public TropicalFishes(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public TropicalFishes clone() {
            return (TropicalFishes) super.clone();
        }

        @Override
        public DyeColor getPatternColor() {
            if (!hasPatternColor()) {
                throw new IllegalStateException("Pattern color is absent");
            }
            return Convert.dye(Nbt.string(c, kPatternColor, "white"));
        }

        @Override
        public void setPatternColor(DyeColor color) {
            put(kPatternColor, color == null ? null : Convert.dye(color));
        }

        @Override
        public DyeColor getBodyColor() {
            if (!hasBodyColor()) {
                throw new IllegalStateException("Body color is absent");
            }
            return Convert.dye(Nbt.string(c, kBody, "white"));
        }

        @Override
        public void setBodyColor(DyeColor color) {
            put(kBody, color == null ? null : Convert.dye(color));
        }

        @Override
        public TropicalFish.Pattern getPattern() {
            if (!hasPattern()) {
                throw new IllegalStateException("Pattern is absent");
            }
            return TropicalFish.Pattern.valueOf(Nbt.string(c, kPattern, "kob").toUpperCase(Locale.ROOT));
        }

        @Override
        public void setPattern(TropicalFish.Pattern pattern) {
            put(kPattern, pattern == null ? null : pattern.name().toLowerCase(Locale.ROOT));
        }

        @Override
        public boolean hasPattern() {
            return c.containsKey(kPattern);
        }

        @Override
        public boolean hasBodyColor() {
            return c.containsKey(kBody);
        }

        @Override
        public boolean hasPatternColor() {
            return c.containsKey(kPatternColor);
        }

        @Override
        public boolean hasVariant() {
            return hasPattern() || hasBodyColor() || hasPatternColor();
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Knowledge extends MetaItem implements KnowledgeBookMeta {
        public Knowledge(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Knowledge clone() {
            return (Knowledge) super.clone();
        }

        @Override
        public boolean hasRecipes() {
            List<Object> list = Nbt.list(c.get("minecraft:recipes"));
            return list != null && !list.isEmpty();
        }

        @Override
        public List<NamespacedKey> getRecipes() {
            List<NamespacedKey> out = new ArrayList<>();
            List<Object> list = Nbt.list(c.get("minecraft:recipes"));
            if (list != null) {
                for (Object entry : list) {
                    NamespacedKey key = NamespacedKey.fromString(String.valueOf(entry));
                    if (key != null) {
                        out.add(key);
                    }
                }
            }
            return out;
        }

        @Override
        public void setRecipes(List<NamespacedKey> recipes) {
            if (recipes == null || recipes.isEmpty()) {
                c.remove("minecraft:recipes");
                return;
            }
            List<Object> list = new ArrayList<>();
            for (NamespacedKey key : recipes) {
                list.add(key.toString());
            }
            c.put("minecraft:recipes", list);
        }

        @Override
        public void addRecipe(NamespacedKey... recipes) {
            List<NamespacedKey> all = getRecipes();
            all.addAll(List.of(recipes));
            setRecipes(all);
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Instruments extends MetaItem implements MusicInstrumentMeta {
        public Instruments(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Instruments clone() {
            return (Instruments) super.clone();
        }

        @Override
        public void setInstrument(org.bukkit.MusicInstrument instrument) {
            put("minecraft:instrument", instrument == null ? null : instrument.getKey().toString());
        }

        @Override
        public org.bukkit.MusicInstrument getInstrument() {
            NamespacedKey key = key("minecraft:instrument");
            return key == null ? null : org.bukkit.MusicInstrument.getByKey(key);
        }
    }

    @DelegateDeserialization(ItemMetas.Serial.class)
    public static final class Ominous extends MetaItem implements OminousBottleMeta {
        private static final String kAmplifier = "minecraft:ominous_bottle_amplifier";

        public Ominous(Material type, Map<String, Object> components) {
            super(type, components);
        }

        @Override
        public Ominous clone() {
            return (Ominous) super.clone();
        }

        @Override
        public boolean hasAmplifier() {
            return c.containsKey(kAmplifier);
        }

        @Override
        public int getAmplifier() {
            if (!hasAmplifier()) {
                throw new IllegalStateException("Amplifier is absent");
            }
            return Nbt.integer(c, kAmplifier, 0);
        }

        @Override
        public void setAmplifier(int amplifier) {
            if (amplifier < 0 || amplifier > 4) {
                throw new IllegalArgumentException("Amplifier must be between 0 and 4");
            }
            c.put(kAmplifier, amplifier);
        }
    }
}
