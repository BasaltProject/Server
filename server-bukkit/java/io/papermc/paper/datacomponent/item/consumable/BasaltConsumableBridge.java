package io.papermc.paper.datacomponent.item.consumable;

import io.papermc.paper.registry.set.RegistryKeySet;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.key.Key;
import org.basaltmc.bukkit.datacomponent.ItemSpecs;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

/** Builds the consume effects the item component API hands out. */
public final class BasaltConsumableBridge implements ConsumableTypesBridge {
    @Override
    public ConsumeEffect.ApplyStatusEffects applyStatusEffects(List<PotionEffect> effects, float probability) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("effects", new ArrayList<Object>(effects));
        fields.put("probability", probability);
        return (ConsumeEffect.ApplyStatusEffects) ItemSpecs.APPLY_EFFECTS.value(fields);
    }

    @Override
    public ConsumeEffect.RemoveStatusEffects removeStatusEffects(RegistryKeySet<PotionEffectType> effects) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("removeEffects", effects);
        return (ConsumeEffect.RemoveStatusEffects) ItemSpecs.REMOVE_EFFECTS.value(fields);
    }

    @Override
    public ConsumeEffect.ClearAllStatusEffects clearAllStatusEffects() {
        return (ConsumeEffect.ClearAllStatusEffects) ItemSpecs.CLEAR_EFFECTS.value(new LinkedHashMap<>());
    }

    @Override
    public ConsumeEffect.PlaySound playSoundEffect(Key sound) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("sound", sound);
        return (ConsumeEffect.PlaySound) ItemSpecs.PLAY_SOUND.value(fields);
    }

    @Override
    public ConsumeEffect.TeleportRandomly teleportRandomlyEffect(float diameter, boolean directionalParticles) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("diameter", diameter);
        fields.put("directionalParticles", directionalParticles);
        return (ConsumeEffect.TeleportRandomly) ItemSpecs.TELEPORT.value(fields);
    }
}
