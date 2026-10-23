package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.bukkit.Color;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.PotionMeta;
import org.bukkit.potion.PotionData;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.potion.PotionType;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaPotion extends MetaItem implements PotionMeta {
    private static final String kContents = "minecraft:potion_contents";
    private static final int kWaterColor = 0x385DC6;

    public MetaPotion(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaPotion clone() {
        return (MetaPotion) super.clone();
    }

    private Map<String, Object> contents(boolean create) {
        Object value = c.get(kContents);
        if (value instanceof String potion) {
            Map<String, Object> converted = Nbt.compound();
            converted.put("potion", potion);
            c.put(kContents, converted);
            return converted;
        }
        Map<String, Object> found = Nbt.map(value);
        if (found == null && create) {
            found = Nbt.compound();
            c.put(kContents, found);
        }
        return found;
    }

    private void tidy() {
        Map<String, Object> found = Nbt.map(c.get(kContents));
        if (found != null && found.isEmpty()) {
            c.remove(kContents);
        }
    }

    @Override
    public void setBasePotionData(PotionData data) {
        if (data == null) {
            throw new IllegalArgumentException("PotionData cannot be null");
        }
        PotionType type = data.getType();
        String name = type.name();
        PotionType resolved = null;
        if (data.isUpgraded()) {
            resolved = typeNamed("STRONG_" + name);
        } else if (data.isExtended()) {
            resolved = typeNamed("LONG_" + name);
        }
        setBasePotionType(resolved != null ? resolved : type);
    }

    private static PotionType typeNamed(String name) {
        try {
            return PotionType.valueOf(name);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    @Override
    public PotionData getBasePotionData() {
        PotionType type = getBasePotionType();
        if (type == null) {
            return new PotionData(PotionType.WATER);
        }
        String name = type.name();
        if (name.startsWith("STRONG_") && typeNamed(name.substring(7)) != null) {
            return new PotionData(typeNamed(name.substring(7)), false, true);
        }
        if (name.startsWith("LONG_") && typeNamed(name.substring(5)) != null) {
            return new PotionData(typeNamed(name.substring(5)), true, false);
        }
        return new PotionData(type);
    }

    @Override
    public void setBasePotionType(PotionType type) {
        if (type == null) {
            Map<String, Object> found = contents(false);
            if (found != null) {
                found.remove("potion");
                tidy();
            }
            return;
        }
        contents(true).put("potion", type.getKey().toString());
    }

    @Override
    public PotionType getBasePotionType() {
        Map<String, Object> found = contents(false);
        String id = found == null ? null : Nbt.string(found, "potion", null);
        NamespacedKey key = id == null ? null : NamespacedKey.fromString(id);
        return key == null ? null : Registry.POTION.get(key);
    }

    @Override
    public boolean hasBasePotionType() {
        return getBasePotionType() != null;
    }

    @Override
    public boolean hasCustomEffects() {
        Map<String, Object> found = contents(false);
        List<Object> list = found == null ? null : Nbt.list(found.get("custom_effects"));
        return list != null && !list.isEmpty();
    }

    @Override
    public List<PotionEffect> getCustomEffects() {
        Map<String, Object> found = contents(false);
        return found == null ? new ArrayList<>() : Convert.effects(found.get("custom_effects"));
    }

    @Override
    public List<PotionEffect> getAllEffects() {
        List<PotionEffect> out = new ArrayList<>();
        PotionType base = getBasePotionType();
        if (base != null) {
            out.addAll(base.getPotionEffects());
        }
        out.addAll(getCustomEffects());
        return out;
    }

    @Override
    public boolean addCustomEffect(PotionEffect effect, boolean overwrite) {
        if (effect == null) {
            throw new IllegalArgumentException("PotionEffect cannot be null");
        }
        List<PotionEffect> effects = getCustomEffects();
        for (int at = 0; at < effects.size(); at++) {
            if (effects.get(at).getType().equals(effect.getType())) {
                if (!overwrite || effects.get(at).equals(effect)) {
                    return false;
                }
                effects.set(at, effect);
                storeEffects(effects);
                return true;
            }
        }
        effects.add(effect);
        storeEffects(effects);
        return true;
    }

    private void storeEffects(List<PotionEffect> effects) {
        if (effects.isEmpty()) {
            Map<String, Object> found = contents(false);
            if (found != null) {
                found.remove("custom_effects");
                tidy();
            }
            return;
        }
        List<Object> list = new ArrayList<>();
        for (PotionEffect effect : effects) {
            list.add(Convert.effect(effect));
        }
        contents(true).put("custom_effects", list);
    }

    @Override
    public boolean removeCustomEffect(PotionEffectType type) {
        List<PotionEffect> effects = getCustomEffects();
        boolean changed = effects.removeIf(effect -> effect.getType().equals(type));
        if (changed) {
            storeEffects(effects);
        }
        return changed;
    }

    @Override
    public boolean hasCustomEffect(PotionEffectType type) {
        for (PotionEffect effect : getCustomEffects()) {
            if (effect.getType().equals(type)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean setMainEffect(PotionEffectType type) {
        List<PotionEffect> effects = getCustomEffects();
        for (int at = 0; at < effects.size(); at++) {
            if (effects.get(at).getType().equals(type)) {
                if (at == 0) {
                    return false;
                }
                effects.add(0, effects.remove(at));
                storeEffects(effects);
                return true;
            }
        }
        throw new IllegalArgumentException("Potion effect type " + type + " is not on this potion");
    }

    @Override
    public boolean clearCustomEffects() {
        boolean had = hasCustomEffects();
        storeEffects(new ArrayList<>());
        return had;
    }

    @Override
    public boolean hasColor() {
        Map<String, Object> found = contents(false);
        return found != null && found.get("custom_color") instanceof Number;
    }

    @Override
    public Color getColor() {
        Map<String, Object> found = contents(false);
        return found != null && found.get("custom_color") instanceof Number rgb ? Color.fromRGB(rgb.intValue() & 0xFFFFFF) : null;
    }

    @Override
    public void setColor(Color color) {
        if (color == null) {
            Map<String, Object> found = contents(false);
            if (found != null) {
                found.remove("custom_color");
                tidy();
            }
            return;
        }
        contents(true).put("custom_color", color.asRGB());
    }

    @Override
    public Color computeEffectiveColor() {
        if (hasColor()) {
            return getColor();
        }
        long red = 0;
        long green = 0;
        long blue = 0;
        int weight = 0;
        for (PotionEffect effect : getAllEffects()) {
            if (!effect.hasParticles()) {
                continue;
            }
            int amplifier = effect.getAmplifier() + 1;
            Color color = effect.getType().getColor();
            red += (long) amplifier * color.getRed();
            green += (long) amplifier * color.getGreen();
            blue += (long) amplifier * color.getBlue();
            weight += amplifier;
        }
        if (weight == 0) {
            return Color.fromRGB(kWaterColor);
        }
        return Color.fromRGB((int) (red / weight), (int) (green / weight), (int) (blue / weight));
    }

    @Override
    public boolean hasCustomPotionName() {
        Map<String, Object> found = contents(false);
        return found != null && found.get("custom_name") instanceof String;
    }

    @Override
    public String getCustomPotionName() {
        Map<String, Object> found = contents(false);
        return found == null ? null : Nbt.string(found, "custom_name", null);
    }

    @Override
    public void setCustomPotionName(String name) {
        if (name == null) {
            Map<String, Object> found = contents(false);
            if (found != null) {
                found.remove("custom_name");
                tidy();
            }
            return;
        }
        contents(true).put("custom_name", name);
    }
}
