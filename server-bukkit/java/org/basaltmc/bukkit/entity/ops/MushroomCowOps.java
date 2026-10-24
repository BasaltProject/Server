package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.MushroomCow;

public interface MushroomCowOps extends MushroomCow, Cored {
    default MushroomCow.Variant getVariant() {
        return "brown".equals(core().nbtText("Type")) ? MushroomCow.Variant.BROWN : MushroomCow.Variant.RED;
    }

    default void setVariant(MushroomCow.Variant variant) {
        core().nbtSet("Type", "string", variant == MushroomCow.Variant.BROWN ? "brown" : "red");
    }

    private java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> stew() {
        java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> out = new java.util.ArrayList<>();
        java.util.List<Object> stored = org.basaltmc.bukkit.item.Nbt.list(EntityNbt.compound(core(), "")
            .get("stew_effects"));
        for (Object entry : stored == null ? java.util.List.of() : stored) {
            java.util.Map<String, Object> map = org.basaltmc.bukkit.item.Nbt.map(entry);
            org.bukkit.potion.PotionEffectType type = map == null ? null : org.bukkit.potion.PotionEffectType
                .getByKey(org.bukkit.NamespacedKey.fromString(org.basaltmc.bukkit.item.Nbt.string(map, "id", "")));
            if (type != null) {
                out.add(io.papermc.paper.potion.SuspiciousEffectEntry.create(type,
                    org.basaltmc.bukkit.item.Nbt.integer(map, "duration", 160)));
            }
        }
        return out;
    }

    private void stew(java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> effects) {
        if (effects.isEmpty()) {
            core().nbtRemove("stew_effects");
            return;
        }
        java.util.List<Object> stored = new java.util.ArrayList<>();
        for (io.papermc.paper.potion.SuspiciousEffectEntry entry : effects) {
            java.util.Map<String, Object> map = org.basaltmc.bukkit.item.Nbt.compound();
            map.put("id", entry.effect().getKey().toString());
            map.put("duration", entry.duration());
            stored.add(map);
        }
        EntityNbt.put(core(), "stew_effects", stored);
    }

    default java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> getStewEffects() {
        return stew();
    }

    default void setStewEffects(java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> effects) {
        stew(new java.util.ArrayList<>(effects));
    }

    default boolean addEffectToNextStew(io.papermc.paper.potion.SuspiciousEffectEntry effect, boolean overwrite) {
        java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> held = stew();
        for (io.papermc.paper.potion.SuspiciousEffectEntry entry : held) {
            if (entry.effect().equals(effect.effect())) {
                if (!overwrite) {
                    return false;
                }
                held.remove(entry);
                break;
            }
        }
        held.add(effect);
        stew(held);
        return true;
    }

    default boolean addEffectToNextStew(org.bukkit.potion.PotionEffect effect, boolean overwrite) {
        return addEffectToNextStew(io.papermc.paper.potion.SuspiciousEffectEntry.create(effect.getType(),
            effect.getDuration()), overwrite);
    }

    default boolean removeEffectFromNextStew(org.bukkit.potion.PotionEffectType type) {
        java.util.List<io.papermc.paper.potion.SuspiciousEffectEntry> held = stew();
        boolean removed = held.removeIf(entry -> entry.effect().equals(type));
        if (removed) {
            stew(held);
        }
        return removed;
    }

    default boolean hasEffectForNextStew(org.bukkit.potion.PotionEffectType type) {
        return stew().stream().anyMatch(entry -> entry.effect().equals(type));
    }

    default boolean hasEffectsForNextStew() {
        return !stew().isEmpty();
    }

    default void clearEffectsForNextStew() {
        stew(new java.util.ArrayList<>());
    }

    default java.util.List<org.bukkit.potion.PotionEffect> getEffectsForNextStew() {
        java.util.List<org.bukkit.potion.PotionEffect> out = new java.util.ArrayList<>();
        for (io.papermc.paper.potion.SuspiciousEffectEntry entry : stew()) {
            out.add(new org.bukkit.potion.PotionEffect(entry.effect(), entry.duration(), 0));
        }
        return out;
    }
}
