package org.basaltmc.bukkit.entity;

import java.util.Locale;
import java.util.Map;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.Color;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeModifier;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionEffectTypeCategory;

/** A status effect named by the server's own registry. */
public final class BasaltPotionEffectType extends PotionEffectType {
    private static final int[] kColors = {3402751, 9154528, 14270531, 4866583, 16762624, 16262179, 11101546, 16646020,
        5578058, 13458603, 9520880, 16750848, 10017472, 16185078, 2039587, 12779366, 5797459, 4738376, 8889187,
        7561558, 16284963, 2445989, 16262179, 9740385, 13565951, 5882118, 12624973, 15978425, 1950417, 8954814,
        745784, 4521796, 2696993, 1484454, 14565464, 12438015, 7891290, 10092451, 9214860, 65518};
    private static final java.util.Set<String> kHarmful = java.util.Set.of("slowness", "mining_fatigue",
        "instant_damage", "nausea", "blindness", "hunger", "weakness", "poison", "wither", "levitation", "unluck",
        "darkness", "bad_omen", "trial_omen", "raid_omen", "infested");
    private static final java.util.Set<String> kNeutral = java.util.Set.of("glowing", "wind_charged", "weaving",
        "oozing");

    private final NamespacedKey key;
    private final int ordinal;

    public BasaltPotionEffectType(NamespacedKey key, int ordinal) {
        this.key = key;
        this.ordinal = ordinal;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public PotionEffect createEffect(int duration, int amplifier) {
        return new PotionEffect(this, isInstant() ? 1 : duration, amplifier);
    }

    @Override
    public boolean isInstant() {
        String path = key.getKey();
        return path.equals("instant_health") || path.equals("instant_damage") || path.equals("saturation");
    }

    @Override
    public PotionEffectTypeCategory getCategory() {
        String path = key.getKey();
        return kHarmful.contains(path) ? PotionEffectTypeCategory.HARMFUL
            : kNeutral.contains(path) ? PotionEffectTypeCategory.NEUTRAL : PotionEffectTypeCategory.BENEFICIAL;
    }

    @Override
    public Category getEffectCategory() {
        String path = key.getKey();
        return kHarmful.contains(path) ? Category.HARMFUL : kNeutral.contains(path) ? Category.NEUTRAL
            : Category.BENEFICIAL;
    }

    @Override
    public Color getColor() {
        return Color.fromRGB(ordinal < kColors.length ? kColors[ordinal] : 0xFFFFFF);
    }

    @Override
    public double getDurationModifier() {
        return isInstant() ? 0.5 : 1.0;
    }

    @Override
    @SuppressWarnings("deprecation")
    public int getId() {
        return ordinal + 1;
    }

    @Override
    @SuppressWarnings("deprecation")
    public String getName() {
        return key.getKey().toUpperCase(Locale.ROOT);
    }

    @Override
    public Map<Attribute, AttributeModifier> getEffectAttributes() {
        throw Unsupported.hit("PotionEffectType#getEffectAttributes");
    }

    @Override
    public double getAttributeModifierAmount(Attribute attribute, int amplifier) {
        throw Unsupported.hit("PotionEffectType#getAttributeModifierAmount");
    }

    @Override
    public String translationKey() {
        return "effect.minecraft." + key.getKey();
    }

    @Override
    public String getTranslationKey() {
        return translationKey();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof PotionEffectType type && type.getKey().equals(key);
    }

    @Override
    public int hashCode() {
        return key.hashCode();
    }

    @Override
    public String toString() {
        return "PotionEffectType[" + key + "]";
    }
}
