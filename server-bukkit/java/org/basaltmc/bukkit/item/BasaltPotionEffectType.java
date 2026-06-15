package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import org.basaltmc.bukkit.InventoryNatives;
import org.bukkit.Color;
import org.bukkit.NamespacedKey;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeModifier;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionEffectTypeCategory;

/** A mob effect with the registry facts the server holds: its protocol id and colour. */
public final class BasaltPotionEffectType extends PotionEffectType {
    private static final Set<String> kBeneficial = Set.of("speed", "haste", "strength", "instant_health", "jump_boost",
        "regeneration", "resistance", "fire_resistance", "water_breathing", "invisibility", "night_vision",
        "health_boost", "absorption", "saturation", "luck", "slow_falling", "conduit_power", "dolphins_grace",
        "hero_of_the_village", "breath_of_the_nautilus");
    private static final Set<String> kNeutral = Set.of("glowing", "bad_omen", "trial_omen", "raid_omen");
    private static final Set<String> kInstant = Set.of("instant_health", "instant_damage", "saturation");
    private static final Map<String, String> kLegacy = Map.ofEntries(Map.entry("slowness", "SLOW"),
        Map.entry("haste", "FAST_DIGGING"), Map.entry("mining_fatigue", "SLOW_DIGGING"),
        Map.entry("strength", "INCREASE_DAMAGE"), Map.entry("instant_health", "HEAL"),
        Map.entry("instant_damage", "HARM"), Map.entry("jump_boost", "JUMP"), Map.entry("nausea", "CONFUSION"),
        Map.entry("resistance", "DAMAGE_RESISTANCE"));

    private final NamespacedKey key;
    private final int id;
    private final int color;

    private BasaltPotionEffectType(NamespacedKey key, int id, int color) {
        this.key = key;
        this.id = id;
        this.color = color;
    }

    public static List<PotionEffectType> all() {
        List<PotionEffectType> out = new ArrayList<>();
        byte[] bytes = InventoryNatives.mobEffects();
        Map<String, Object> root = bytes == null ? null : Nbt.read(bytes);
        List<Object> entries = root == null ? null : Nbt.list(root.get("effects"));
        if (entries != null) {
            for (Object entry : entries) {
                Map<String, Object> data = Nbt.map(entry);
                if (data != null) {
                    out.add(new BasaltPotionEffectType(NamespacedKey.fromString(Nbt.string(data, "id", "")),
                        Nbt.integer(data, "protocol_id", 0) + 1, Nbt.integer(data, "color", 0)));
                }
            }
        }
        return out;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public PotionEffect createEffect(int duration, int amplifier) {
        return new PotionEffect(this, isInstant() ? 1 : (int) (duration * getDurationModifier()), amplifier);
    }

    @Override
    public boolean isInstant() {
        return kInstant.contains(key.getKey());
    }

    @Override
    public PotionEffectTypeCategory getCategory() {
        if (kBeneficial.contains(key.getKey())) {
            return PotionEffectTypeCategory.BENEFICIAL;
        }
        return kNeutral.contains(key.getKey()) ? PotionEffectTypeCategory.NEUTRAL : PotionEffectTypeCategory.HARMFUL;
    }

    @Override
    public Category getEffectCategory() {
        PotionEffectTypeCategory category = getCategory();
        return category == PotionEffectTypeCategory.BENEFICIAL ? Category.BENEFICIAL
            : category == PotionEffectTypeCategory.NEUTRAL ? Category.NEUTRAL : Category.HARMFUL;
    }

    @Override
    public Color getColor() {
        return Color.fromRGB(color & 0xFFFFFF);
    }

    @Override
    public double getDurationModifier() {
        return 1.0;
    }

    @Override
    public int getId() {
        return id;
    }

    @Override
    public String getName() {
        String legacy = kLegacy.get(key.getKey());
        return legacy != null ? legacy : key.getKey().toUpperCase(Locale.ROOT);
    }

    @Override
    public Map<Attribute, AttributeModifier> getEffectAttributes() {
        return Map.of();
    }

    @Override
    public double getAttributeModifierAmount(Attribute attribute, int effectAmplifier) {
        throw new IllegalArgumentException(this + " does not have " + attribute + ". Make sure to check getEffectAttributes first");
    }

    @Override
    public String getTranslationKey() {
        return translationKey();
    }

    @Override
    public String translationKey() {
        return "effect." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltPotionEffectType type && type.key.equals(key);
    }

    @Override
    public int hashCode() {
        return key.hashCode();
    }

    @Override
    public String toString() {
        return "PotionEffectType[" + id + ", " + getName() + "]";
    }
}
