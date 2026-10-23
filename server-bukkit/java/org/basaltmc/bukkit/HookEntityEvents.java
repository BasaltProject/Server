package org.basaltmc.bukkit;

import java.util.LinkedHashMap;
import java.util.Map;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Item;
import org.bukkit.entity.LivingEntity;

/** The entity events raised from deep in the game systems, as the Bukkit and Paper events plugins listen for. */
final class HookEntityEvents {
    private HookEntityEvents() {}

    private static Entity entityAt(EventBridge.Fields f, String field) {
        BasaltWorld world = f.world();
        return world == null ? null : world.entity((int) f.whole(field));
    }

    static void install() {
        EventBridge.map("entity.potion_splash", org.bukkit.event.entity.PotionSplashEvent.class, f -> {
            if (!(f.entity() instanceof org.bukkit.entity.ThrownPotion potion)) {
                return null;
            }
            Map<LivingEntity, Double> affected = new LinkedHashMap<>();
            for (String line : f.text("affected").split("\n")) {
                int bar = line.indexOf('|');
                if (bar > 0 && entityAt(f, "entity_id") != null) {
                    Entity hit = f.world().entity(Integer.parseInt(line.substring(0, bar)));
                    if (hit instanceof LivingEntity living) {
                        affected.put(living, Double.parseDouble(line.substring(bar + 1)));
                    }
                }
            }
            return new org.bukkit.event.entity.PotionSplashEvent(potion, affected);
        }, (e, f) -> {
            org.bukkit.event.entity.PotionSplashEvent splash = (org.bukkit.event.entity.PotionSplashEvent) e;
            StringBuilder lines = new StringBuilder();
            for (LivingEntity hit : splash.getAffectedEntities()) {
                double intensity = splash.getIntensity(hit);
                if (intensity > 0) {
                    lines.append(lines.length() == 0 ? "" : "\n").append(hit.getEntityId()).append('|').append(intensity);
                }
            }
            f.put("affected", lines.toString());
        });
        EventBridge.map("entity.exp_bottle", org.bukkit.event.entity.ExpBottleEvent.class,
            f -> f.entity() instanceof org.bukkit.entity.ThrownExpBottle bottle
                ? new org.bukkit.event.entity.ExpBottleEvent(bottle, (int) f.whole("experience")) : null,
            (e, f) -> f.put("experience", (long) ((org.bukkit.event.entity.ExpBottleEvent) e).getExperience()));
        EventBridge.map("item.merge", org.bukkit.event.entity.ItemMergeEvent.class, f -> {
            Entity target = entityAt(f, "entity_id");
            Entity source = entityAt(f, "source_id");
            return target instanceof Item into && source instanceof Item from
                ? new org.bukkit.event.entity.ItemMergeEvent(from, into) : null;
        });
        EventBridge.map("item.despawn", org.bukkit.event.entity.ItemDespawnEvent.class, f -> {
            Entity item = entityAt(f, "entity_id");
            return item instanceof Item dropped
                ? new org.bukkit.event.entity.ItemDespawnEvent(dropped, f.location()) : null;
        });
        EventBridge.map("entity.change_block", org.bukkit.event.entity.EntityChangeBlockEvent.class, f -> {
            Entity who = f.entity();
            Material to = Material.matchMaterial(f.text("to_block"));
            return who == null || to == null ? null
                : new org.bukkit.event.entity.EntityChangeBlockEvent(who, f.block(), to.createBlockData());
        });
        EventBridge.map("entity.teleport", org.bukkit.event.entity.EntityTeleportEvent.class, f -> {
            Entity who = f.entity();
            return who == null ? null : new org.bukkit.event.entity.EntityTeleportEvent(who,
                new Location(f.world(), f.real("x"), f.real("y"), f.real("z")),
                new Location(f.world(), f.real("to_x"), f.real("to_y"), f.real("to_z")));
        });
        EventBridge.map("entity.unleash", org.bukkit.event.entity.EntityUnleashEvent.class, f -> {
            Entity who = f.entity();
            if (who == null || f.text("reason").equals("player")) {
                return null;
            }
            org.bukkit.event.entity.EntityUnleashEvent.UnleashReason reason = switch (f.text("reason")) {
                case "holder_gone" -> org.bukkit.event.entity.EntityUnleashEvent.UnleashReason.HOLDER_GONE;
                case "distance" -> org.bukkit.event.entity.EntityUnleashEvent.UnleashReason.DISTANCE;
                default -> org.bukkit.event.entity.EntityUnleashEvent.UnleashReason.UNKNOWN;
            };
            return new org.bukkit.event.entity.EntityUnleashEvent(who, reason, f.flag("drop"));
        });
        EventBridge.map("player.leash", org.bukkit.event.entity.PlayerLeashEntityEvent.class, f -> {
            Entity who = f.entity();
            return who == null || f.player() == null ? null
                : new org.bukkit.event.entity.PlayerLeashEntityEvent(who, f.player(), f.player());
        });
        EventBridge.map("player.unleash", org.bukkit.event.player.PlayerUnleashEntityEvent.class, f -> {
            Entity who = f.entity();
            return who == null || f.player() == null ? null
                : new org.bukkit.event.player.PlayerUnleashEntityEvent(who, f.player());
        });
        EventBridge.map("entity.toggle_glide", org.bukkit.event.entity.EntityToggleGlideEvent.class,
            f -> f.entity() instanceof LivingEntity who
                ? new org.bukkit.event.entity.EntityToggleGlideEvent(who, f.flag("gliding")) : null);
        EventBridge.map("entity.air_change", org.bukkit.event.entity.EntityAirChangeEvent.class,
            f -> f.entity() == null ? null
                : new org.bukkit.event.entity.EntityAirChangeEvent(f.entity(), (int) f.whole("amount")),
            (e, f) -> f.put("amount", (long) ((org.bukkit.event.entity.EntityAirChangeEvent) e).getAmount()));
        EventBridge.map("entity.orb_merge", com.destroystokyo.paper.event.entity.ExperienceOrbMergeEvent.class, f -> {
            Entity target = f.entity();
            Entity source = entityAt(f, "source_id");
            return target instanceof org.bukkit.entity.ExperienceOrb into
                && source instanceof org.bukkit.entity.ExperienceOrb from
                ? new com.destroystokyo.paper.event.entity.ExperienceOrbMergeEvent(into, from) : null;
        });
        EventBridge.map("entity.transform", org.bukkit.event.entity.EntityTransformEvent.class, f -> {
            Entity original = f.entity();
            org.bukkit.entity.EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("to_type"));
            if (original == null || type == org.bukkit.entity.EntityType.UNKNOWN) {
                return null;
            }
            Entity made = org.basaltmc.bukkit.entity.EntityCore.template(f.world(), type, f.real("x"), f.real("y"),
                f.real("z"), 0f, 0f);
            org.bukkit.event.entity.EntityTransformEvent.TransformReason reason = switch (type) {
                case ZOMBIE_VILLAGER -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.INFECTION;
                case DROWNED -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.DROWNED;
                case ZOMBIFIED_PIGLIN -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.PIGLIN_ZOMBIFIED;
                case WITCH -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.LIGHTNING;
                case STRAY -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.FROZEN;
                case VILLAGER -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.CURED;
                default -> org.bukkit.event.entity.EntityTransformEvent.TransformReason.UNKNOWN;
            };
            return new org.bukkit.event.entity.EntityTransformEvent(original, java.util.List.of(made), reason);
        });
        EventBridge.map("entity.split", org.bukkit.event.entity.SlimeSplitEvent.class,
            f -> f.entity() instanceof org.bukkit.entity.AbstractCubeMob cube
                ? new org.bukkit.event.entity.SlimeSplitEvent(cube, (int) f.whole("count")) : null,
            (e, f) -> f.put("count", (long) ((org.bukkit.event.entity.SlimeSplitEvent) e).getCount()));
        EventBridge.map("entity.piglin_barter", org.bukkit.event.entity.PiglinBarterEvent.class, f -> {
            java.util.List<org.bukkit.inventory.ItemStack> outcome = new java.util.ArrayList<>();
            for (String line : f.text("outcome").split("\n")) {
                int bar = line.indexOf('|');
                Material material = bar < 0 ? null : Material.matchMaterial(line.substring(0, bar));
                if (material != null && !material.isAir()) {
                    outcome.add(new org.bukkit.inventory.ItemStack(material, Integer.parseInt(line.substring(bar + 1))));
                }
            }
            return f.entity() instanceof org.bukkit.entity.Piglin piglin
                ? new org.bukkit.event.entity.PiglinBarterEvent(piglin,
                    new org.bukkit.inventory.ItemStack(Material.GOLD_INGOT), outcome) : null;
        }, (e, f) -> {
            StringBuilder lines = new StringBuilder();
            for (org.bukkit.inventory.ItemStack stack : ((org.bukkit.event.entity.PiglinBarterEvent) e).getOutcome()) {
                if (stack != null && !stack.getType().isAir() && stack.getAmount() > 0) {
                    lines.append(lines.length() == 0 ? "" : "\n").append(stack.getType().getKey()).append('|')
                        .append(stack.getAmount());
                }
            }
            f.put("outcome", lines.toString());
        });
        EventBridge.map("entity.firework_explode", org.bukkit.event.entity.FireworkExplodeEvent.class,
            f -> f.entity() instanceof org.bukkit.entity.Firework rocket
                ? new org.bukkit.event.entity.FireworkExplodeEvent(rocket) : null);
        EventBridge.map("entity.toggle_swim", org.bukkit.event.entity.EntityToggleSwimEvent.class,
            f -> f.entity() instanceof LivingEntity who
                ? new org.bukkit.event.entity.EntityToggleSwimEvent(who, f.flag("swimming")) : null);
        EventBridge.map("entity.portal_enter", org.bukkit.event.entity.EntityPortalEnterEvent.class,
            f -> f.entity() == null ? null
                : new org.bukkit.event.entity.EntityPortalEnterEvent(f.entity(), f.location()));
        EventBridge.map("entity.enter_block", org.bukkit.event.entity.EntityEnterBlockEvent.class, f -> {
            Entity who = f.entity();
            return who == null || f.world() == null ? null : new org.bukkit.event.entity.EntityEnterBlockEvent(who,
                f.world().getBlockAt((int) f.whole("block_x"), (int) f.whole("block_y"), (int) f.whole("block_z")));
        });
    }
}
