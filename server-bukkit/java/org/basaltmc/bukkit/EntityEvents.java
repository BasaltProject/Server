package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Locale;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.damage.DamageSource;
import org.bukkit.damage.DamageType;
import org.bukkit.event.Event;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Mob;
import org.bukkit.entity.Player;
import org.bukkit.event.entity.CreatureSpawnEvent;
import org.bukkit.event.entity.EntityCombustEvent;
import org.bukkit.event.entity.EntityDamageByEntityEvent;
import org.bukkit.event.entity.EntityDamageEvent;
import org.bukkit.event.entity.EntityDeathEvent;
import org.bukkit.event.entity.EntityMountEvent;
import org.bukkit.event.entity.EntityPotionEffectEvent;
import org.bukkit.event.entity.EntityRegainHealthEvent;
import org.bukkit.event.entity.EntitySpawnEvent;
import org.bukkit.event.entity.EntityTameEvent;
import org.bukkit.event.entity.EntityTargetEvent;
import org.bukkit.event.entity.EntityTargetLivingEntityEvent;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

/** The entity events Basalt raises, as the Bukkit events plugins listen for. */
final class EntityEvents {
    private EntityEvents() {}

    static void install() {
        EventBridge.map("entity.damage", EntityDamageEvent.class, EntityEvents::damage,
            (e, f) -> f.put("amount", ((EntityDamageEvent) e).getDamage()));
        EventBridge.map("entity.drops", EntityDeathEvent.class, f -> {
            Entity dead = f.entity();
            if (!(dead instanceof LivingEntity living)) {
                return null;
            }
            ArrayList<org.bukkit.inventory.ItemStack> drops = new ArrayList<>();
            for (String line : f.text("drops").split("\n")) {
                int bar = line.indexOf('|');
                org.bukkit.Material material = bar < 0 ? null : org.bukkit.Material.matchMaterial(line.substring(0, bar));
                if (material != null && !material.isAir()) {
                    drops.add(new org.bukkit.inventory.ItemStack(material, Integer.parseInt(line.substring(bar + 1))));
                }
            }
            return new EntityDeathEvent(living, DamageSource.builder(DamageType.GENERIC).build(), drops,
                (int) f.whole("exp"));
        }, (e, f) -> {
            EntityDeathEvent death = (EntityDeathEvent) e;
            StringBuilder lines = new StringBuilder();
            for (org.bukkit.inventory.ItemStack stack : death.getDrops()) {
                if (stack != null && !stack.getType().isAir() && stack.getAmount() > 0) {
                    lines.append(lines.length() == 0 ? "" : "\n").append(stack.getType().getKey()).append('|')
                        .append(stack.getAmount());
                }
            }
            f.put("drops", lines.toString());
            f.put("exp", (long) death.getDroppedExp());
        });
        EventBridge.map("entity.spawn", CreatureSpawnEvent.class, f -> {
            if (!mobType(f.text("entity_type"))) {
                return null;
            }
            Entity made = template(f);
            return made instanceof Mob ? new CreatureSpawnEvent((LivingEntity) made, spawnReason(f.text("reason")))
                : null;
        });
        EventBridge.map("entity.spawn", EntitySpawnEvent.class, f -> {
            if (mobType(f.text("entity_type"))) {
                return null;
            }
            Entity made = template(f);
            return made == null ? null : new EntitySpawnEvent(made);
        });
        EventBridge.map("entity.add", com.destroystokyo.paper.event.entity.EntityAddToWorldEvent.class, f -> {
            Entity added = f.entity();
            if (added instanceof Cored real) {
                Entity template = pending.remove(key(f));
                if (template instanceof Cored staged) {
                    staged.core().replayOnto(real.core());
                }
            }
            return added == null ? null : new com.destroystokyo.paper.event.entity.EntityAddToWorldEvent(added,
                added.getWorld());
        });
        EventBridge.map("item.spawn", org.bukkit.event.entity.ItemSpawnEvent.class, f -> {
            BasaltWorld world = f.world();
            org.bukkit.Material material = org.bukkit.Material.matchMaterial(f.text("item"));
            if (world == null || material == null || material.isAir()) {
                return null;
            }
            Entity item = org.basaltmc.bukkit.entity.EntityCore.template(world, EntityType.ITEM, f.real("x"),
                f.real("y"), f.real("z"), 0f, 0f);
            ((Cored) item).core().stageNbt("Item", org.basaltmc.bukkit.entity.EntityItems.toNbt(
                new org.bukkit.inventory.ItemStack(material, (int) Math.max(1, f.whole("count")))));
            return new org.bukkit.event.entity.ItemSpawnEvent((org.bukkit.entity.Item) item);
        });
        EventBridge.map("block.explode", org.bukkit.event.entity.ExplosionPrimeEvent.class, f -> {
            Entity source = f.whole("entity_id") > 0 ? f.entity() : null;
            return source == null ? null : new org.bukkit.event.entity.ExplosionPrimeEvent(source,
                (float) f.real("power"), false);
        }, (e, f) -> f.put("power", (double) ((org.bukkit.event.entity.ExplosionPrimeEvent) e).getRadius()));
        EventBridge.map("block.explode", org.bukkit.event.entity.EntityExplodeEvent.class, f -> {
            Entity source = f.whole("entity_id") > 0 ? f.entity() : null;
            return source == null ? null : new org.bukkit.event.entity.EntityExplodeEvent(source, f.location(),
                new ArrayList<>(), 1.0f, org.bukkit.ExplosionResult.DESTROY);
        });
        EventBridge.map("entity.mount", org.bukkit.event.vehicle.VehicleEnterEvent.class, f -> {
            Entity mount = f.entity();
            return mount instanceof org.bukkit.entity.Vehicle vehicle && f.player() != null && f.flag("mounting")
                ? new org.bukkit.event.vehicle.VehicleEnterEvent(vehicle, f.player()) : null;
        });
        EventBridge.map("entity.heal", EntityRegainHealthEvent.class, f -> {
            Entity healed = f.entity();
            return healed == null ? null : new EntityRegainHealthEvent(healed, f.real("amount"),
                regainReason(f.text("cause")));
        }, (e, f) -> f.put("amount", ((EntityRegainHealthEvent) e).getAmount()));
        EventBridge.map("entity.target", EntityTargetEvent.class, f -> {
            Entity hunter = f.entity();
            if (hunter == null) {
                return null;
            }
            Entity target = f.world().entity((int) f.whole("target_id"));
            if (target == null && f.whole("target_id") >= 0) {
                target = BasaltServer.instance().entityById(f.world(), (int) f.whole("target_id"));
            }
            EntityTargetEvent.TargetReason reason = target == null ? EntityTargetEvent.TargetReason.FORGOT_TARGET
                : target instanceof Player ? EntityTargetEvent.TargetReason.CLOSEST_PLAYER
                : EntityTargetEvent.TargetReason.CLOSEST_ENTITY;
            return target instanceof LivingEntity living ? new EntityTargetLivingEntityEvent(hunter, living, reason)
                : new EntityTargetEvent(hunter, target, reason);
        }, (e, f) -> {
            Entity target = ((EntityTargetEvent) e).getTarget();
            f.put("target_id", (long) (target == null ? -1 : target.getEntityId()));
        });
        EventBridge.map("entity.combust", EntityCombustEvent.class, f -> {
            Entity burning = f.entity();
            return burning == null ? null : new EntityCombustEvent(burning, f.whole("ticks") / 20.0f);
        });
        EventBridge.map("entity.potion_effect", EntityPotionEffectEvent.class, f -> {
            Entity affected = f.entity();
            if (!(affected instanceof LivingEntity living)) {
                return null;
            }
            PotionEffectType type = Registry.EFFECT.get(NamespacedKey.fromString(f.text("effect")));
            if (type == null) {
                return null;
            }
            PotionEffect fresh = new PotionEffect(type, (int) f.whole("duration"), (int) f.whole("amplifier"));
            String action = f.text("action");
            PotionEffect old = living.getPotionEffect(type);
            return new EntityPotionEffectEvent(living, old, fresh, null, EntityPotionEffectEvent.Cause.UNKNOWN,
                action.equals("changed") ? EntityPotionEffectEvent.Action.CHANGED
                    : EntityPotionEffectEvent.Action.ADDED, false);
        });
        EventBridge.map("entity.tame", EntityTameEvent.class, f -> {
            Entity animal = f.entity();
            return animal instanceof LivingEntity living && f.player() != null
                ? new EntityTameEvent(living, f.player()) : null;
        });
        EventBridge.map("entity.mount", EntityMountEvent.class, f -> {
            Entity mount = f.entity();
            return mount == null || f.player() == null || !f.flag("mounting") ? null
                : new EntityMountEvent(f.player(), mount);
        });
    }

    private static final java.util.Map<String, Entity> pending = new java.util.concurrent.ConcurrentHashMap<>();
    private static volatile boolean following;

    private static String key(EventBridge.Fields f) {
        return f.text("world") + ":" + f.text("entity_type") + ":" + Math.round(f.real("x") * 1000.0) + ":"
            + Math.round(f.real("y") * 1000.0) + ":" + Math.round(f.real("z") * 1000.0);
    }

    private static boolean mobType(String key) {
        EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(key);
        return type != EntityType.UNKNOWN && type.getEntityClass() != null
            && Mob.class.isAssignableFrom(type.getEntityClass());
    }

    private static Entity template(EventBridge.Fields f) {
        EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("entity_type"));
        BasaltWorld world = f.world();
        if (type == EntityType.UNKNOWN || world == null) {
            return null;
        }
        if (!following) {
            following = true;
            Natives.subscribe("entity.add", org.bukkit.event.EventPriority.NORMAL.ordinal());
        }
        if (pending.size() > 256) {
            pending.clear();
        }
        Entity made = org.basaltmc.bukkit.entity.EntityCore.template(world, type, f.real("x"), f.real("y"),
            f.real("z"), 0f, 0f);
        pending.put(key(f), made);
        return made;
    }

    private static CreatureSpawnEvent.SpawnReason spawnReason(String name) {
        try {
            return CreatureSpawnEvent.SpawnReason.valueOf(name.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return switch (name) {
                case "event" -> CreatureSpawnEvent.SpawnReason.RAID;
                case "plugin" -> CreatureSpawnEvent.SpawnReason.CUSTOM;
                case "triggered" -> CreatureSpawnEvent.SpawnReason.TRAP;
                default -> CreatureSpawnEvent.SpawnReason.DEFAULT;
            };
        }
    }

    private static EntityRegainHealthEvent.RegainReason regainReason(String name) {
        return switch (name) {
            case "regeneration" -> EntityRegainHealthEvent.RegainReason.REGEN;
            case "eating" -> EntityRegainHealthEvent.RegainReason.EATING;
            case "satiated" -> EntityRegainHealthEvent.RegainReason.SATIATED;
            case "magic" -> EntityRegainHealthEvent.RegainReason.MAGIC;
            case "magic_regen" -> EntityRegainHealthEvent.RegainReason.MAGIC_REGEN;
            default -> EntityRegainHealthEvent.RegainReason.CUSTOM;
        };
    }

    private static Event damage(EventBridge.Fields f) {
        Entity target = f.entity();
        if (target == null) {
            return null;
        }
        String cause = f.text("cause");
        EntityDamageEvent.DamageCause mapped = damageCause(cause);
        DamageType type = Registry.DAMAGE_TYPE.get(NamespacedKey.minecraft(cause.toLowerCase(Locale.ROOT)));
        Entity attacker = f.whole("attacker_id") >= 0 ? BasaltServer.instance().entityById(f.world(),
            (int) f.whole("attacker_id")) : null;
        if (attacker == null && !f.text("attacker_name").isEmpty()) {
            attacker = BasaltServer.instance().playerByName(f.text("attacker_name"));
        }
        DamageSource.Builder builder = DamageSource.builder(type == null ? DamageType.GENERIC : type);
        if (attacker != null) {
            builder.withCausingEntity(attacker).withDirectEntity(attacker);
        }
        DamageSource source = builder.build();
        Event event = attacker == null ? new EntityDamageEvent(target, mapped, source, f.real("amount"))
            : new EntityDamageByEntityEvent(attacker, target, mapped, source, f.real("amount"));
        if (target instanceof Cored cored) {
            cored.core().lastDamageCause = (EntityDamageEvent) event;
        }
        return event;
    }

    static EntityDamageEvent.DamageCause damageCause(String name) {
        return switch (name) {
            case "attack", "player_attack", "mob_attack", "mob_attack_no_aggro", "mace_smash", "stalagmite" ->
                EntityDamageEvent.DamageCause.ENTITY_ATTACK;
            case "projectile", "arrow", "trident", "mob_projectile", "thrown", "spit", "wind_charge", "fireball",
                "unattributed_fireball", "wither_skull", "sting" -> EntityDamageEvent.DamageCause.PROJECTILE;
            case "in_fire", "campfire" -> EntityDamageEvent.DamageCause.FIRE;
            case "on_fire", "fire" -> EntityDamageEvent.DamageCause.FIRE_TICK;
            case "lava" -> EntityDamageEvent.DamageCause.LAVA;
            case "drown" -> EntityDamageEvent.DamageCause.DROWNING;
            case "dry_out" -> EntityDamageEvent.DamageCause.DRYOUT;
            case "fall", "ender_pearl" -> EntityDamageEvent.DamageCause.FALL;
            case "fly_into_wall" -> EntityDamageEvent.DamageCause.FLY_INTO_WALL;
            case "in_wall" -> EntityDamageEvent.DamageCause.SUFFOCATION;
            case "cramming" -> EntityDamageEvent.DamageCause.CRAMMING;
            case "explosion", "player_explosion", "fireworks" -> EntityDamageEvent.DamageCause.ENTITY_EXPLOSION;
            case "out_of_world" -> EntityDamageEvent.DamageCause.VOID;
            case "generic_kill" -> EntityDamageEvent.DamageCause.KILL;
            case "lightning_bolt" -> EntityDamageEvent.DamageCause.LIGHTNING;
            case "magic", "indirect_magic" -> EntityDamageEvent.DamageCause.MAGIC;
            case "wither" -> EntityDamageEvent.DamageCause.WITHER;
            case "starve" -> EntityDamageEvent.DamageCause.STARVATION;
            case "cactus", "sweet_berry_bush" -> EntityDamageEvent.DamageCause.CONTACT;
            case "falling_block", "falling_anvil", "falling_stalactite" -> EntityDamageEvent.DamageCause.FALLING_BLOCK;
            case "thorns" -> EntityDamageEvent.DamageCause.THORNS;
            case "dragon_breath" -> EntityDamageEvent.DamageCause.DRAGON_BREATH;
            case "hot_floor" -> EntityDamageEvent.DamageCause.HOT_FLOOR;
            case "freeze" -> EntityDamageEvent.DamageCause.FREEZE;
            case "sonic_boom" -> EntityDamageEvent.DamageCause.SONIC_BOOM;
            case "outside_border" -> EntityDamageEvent.DamageCause.WORLD_BORDER;
            default -> EntityDamageEvent.DamageCause.CUSTOM;
        };
    }
}
