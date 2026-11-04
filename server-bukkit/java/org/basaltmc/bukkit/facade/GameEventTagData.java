package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.MappedRegistry;
import net.minecraft.resources.Identifier;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.gameevent.GameEvent;

/** parity: `GameEventTagsProvider`, the membership of the game event tags. */
public final class GameEventTagData {
    private static final List<String> kVibrationsExceptFlap = List.of("block_attach", "block_change", "block_close",
        "block_destroy", "block_detach", "block_open", "block_place", "block_activate", "block_deactivate", "bounce",
        "container_close", "container_open", "drink", "eat", "elytra_glide", "entity_damage", "entity_die",
        "entity_dismount", "entity_interact", "entity_mount", "entity_place", "entity_action", "equip", "explode",
        "fluid_pickup", "fluid_place", "hit_ground", "instrument_play", "item_interact_finish", "lightning_strike",
        "note_block_play", "prime_fuse", "projectile_land", "projectile_shoot", "shear", "splash", "step", "swim",
        "teleport", "unequip");

    private GameEventTagData() {}

    public static void bind(MappedRegistry<GameEvent> registry) {
        Holder<GameEvent> loaded = GameEvent.STEP;
        List<String> resonance = new ArrayList<>();
        for (int i = 1; i <= 15; i++) {
            resonance.add("resonate_" + i);
        }
        List<String> vibrations = new ArrayList<>(kVibrationsExceptFlap);
        vibrations.addAll(resonance);
        List<String> warden = new ArrayList<>(vibrations);
        vibrations.add("flap");
        warden.add("shriek");
        warden.add("sculk_sensor_tendrils_clicking");
        put(registry, "vibrations", vibrations);
        put(registry, "shrieker_can_listen", List.of("sculk_sensor_tendrils_clicking"));
        put(registry, "warden_can_listen", warden);
        put(registry, "ignore_vibrations_sneaking", List.of("hit_ground", "projectile_shoot", "step", "swim",
            "item_interact_start", "item_interact_finish"));
        put(registry, "allay_can_listen", List.of("note_block_play"));
    }

    private static void put(MappedRegistry<GameEvent> registry, String tag, List<String> members) {
        List<Holder<GameEvent>> holders = new ArrayList<>();
        for (String member : members) {
            registry.get(Identifier.withDefaultNamespace(member)).ifPresent(holders::add);
        }
        registry.bindTag(TagKey.create(registry.key(), Identifier.withDefaultNamespace(tag)), holders);
    }
}
