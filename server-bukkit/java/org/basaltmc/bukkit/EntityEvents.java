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
import org.bukkit.event.entity.EntityDamageByBlockEvent;
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
        EventBridge.mapLazily("entity.goal", BasaltMobGoals.INSTANCE::answer);
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
        EventBridge.map("player.item_pickup", org.bukkit.event.entity.EntityPickupItemEvent.class, f -> {
            Entity item = f.world() == null ? null : f.world().entity((int) f.whole("entity_id"));
            return item instanceof org.bukkit.entity.Item picked && f.player() != null
                ? new org.bukkit.event.entity.EntityPickupItemEvent(f.player(), picked, 0) : null;
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

        EventBridge.map("entity.mount", org.bukkit.event.vehicle.VehicleExitEvent.class, f -> {
            Entity mount = f.entity();
            return mount instanceof org.bukkit.entity.Vehicle vehicle && f.player() != null && !f.flag("mounting")
                ? new org.bukkit.event.vehicle.VehicleExitEvent(vehicle, f.player()) : null;
        });
        EventBridge.map("entity.mount", org.bukkit.event.entity.EntityDismountEvent.class, f -> {
            Entity mount = f.entity();
            return mount == null || f.player() == null || f.flag("mounting") ? null
                : new org.bukkit.event.entity.EntityDismountEvent(f.player(), mount);
        });
        EventBridge.map("hanging.place", org.bukkit.event.hanging.HangingPlaceEvent.class, f -> {
            EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("entity_type"));
            BasaltWorld world = f.world();
            if (world == null || f.player() == null || type == EntityType.UNKNOWN) {
                return null;
            }
            Entity made = org.basaltmc.bukkit.entity.EntityCore.template(world, type, f.whole("x") + 0.5, f.whole("y"),
                f.whole("z") + 0.5, 0f, 0f);
            return made instanceof org.bukkit.entity.Hanging hanging ? new org.bukkit.event.hanging.HangingPlaceEvent(
                hanging, f.player(), f.block(), PlayerEvents.face(f.text("face")),
                org.bukkit.inventory.EquipmentSlot.HAND, new org.bukkit.inventory.ItemStack(
                    type == EntityType.PAINTING ? org.bukkit.Material.PAINTING
                        : type == EntityType.GLOW_ITEM_FRAME ? org.bukkit.Material.GLOW_ITEM_FRAME
                        : org.bukkit.Material.ITEM_FRAME)) : null;
        });
        EventBridge.map("hanging.break", org.bukkit.event.hanging.HangingBreakByEntityEvent.class, f -> {
            Entity hanging = f.entity();
            Entity remover = BasaltServer.instance().entityById(f.world(), (int) f.whole("remover_id"));
            return hanging instanceof org.bukkit.entity.Hanging made && remover != null
                ? new org.bukkit.event.hanging.HangingBreakByEntityEvent(made, remover,
                    DamageSource.builder(DamageType.PLAYER_ATTACK).build()) : null;
        });

        EventBridge.map("entity.shoot", org.bukkit.event.entity.EntityShootBowEvent.class, f -> {
            Entity shooter = f.entity();
            BasaltWorld world = f.world();
            EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("projectile"));
            if (!(shooter instanceof LivingEntity living) || world == null || type == EntityType.UNKNOWN) {
                return null;
            }
            Entity arrow = org.basaltmc.bukkit.entity.EntityCore.template(world, type, shooter.getLocation().getX(),
                shooter.getLocation().getY() + 1.5, shooter.getLocation().getZ(), 0f, 0f);
            org.bukkit.inventory.ItemStack bow = living.getEquipment() == null ? null
                : living.getEquipment().getItemInMainHand();
            return new org.bukkit.event.entity.EntityShootBowEvent(living, bow, arrow, (float) f.real("power"));
        });
        EventBridge.map("entity.breed", org.bukkit.event.entity.EntityBreedEvent.class, f -> {
            Entity mother = f.entity();
            Entity father = BasaltServer.instance().entityById(f.world(), (int) f.whole("partner_id"));
            EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(f.text("entity_type"));
            if (!(mother instanceof LivingEntity m) || !(father instanceof LivingEntity d) || f.world() == null
                || type == EntityType.UNKNOWN) {
                return null;
            }
            Entity child = org.basaltmc.bukkit.entity.EntityCore.template(f.world(), type, m.getLocation().getX(),
                m.getLocation().getY(), m.getLocation().getZ(), 0f, 0f);
            String named = f.text("breeder");
            LivingEntity breeder = named.isEmpty() ? null : org.bukkit.Bukkit.getPlayerExact(named);
            return child instanceof LivingEntity baby ? new org.bukkit.event.entity.EntityBreedEvent(baby, m, d, breeder,
                null, (int) f.whole("experience")) : null;
        }, (e, f) -> f.put("experience", (long) ((org.bukkit.event.entity.EntityBreedEvent) e).getExperience()));

        EventBridge.map("projectile.hit", org.bukkit.event.entity.ProjectileHitEvent.class, f -> {
            Entity shot = f.entity();
            if (!(shot instanceof org.bukkit.entity.Projectile projectile)) {
                return null;
            }
            if (f.whole("hit_id") >= 0) {
                Entity struck = BasaltServer.instance().entityById(f.world(), (int) f.whole("hit_id"));
                return struck == null ? new org.bukkit.event.entity.ProjectileHitEvent(projectile)
                    : new org.bukkit.event.entity.ProjectileHitEvent(projectile, struck);
            }
            return new org.bukkit.event.entity.ProjectileHitEvent(projectile, f.block());
        });
        EventBridge.map("entity.resurrect", org.bukkit.event.entity.EntityResurrectEvent.class, f -> {
            Entity who = f.entity();
            return who instanceof LivingEntity living ? new org.bukkit.event.entity.EntityResurrectEvent(living,
                f.text("hand").equals("off_hand") ? org.bukkit.inventory.EquipmentSlot.OFF_HAND
                    : org.bukkit.inventory.EquipmentSlot.HAND) : null;
        });
        EventBridge.map("entity.launch", org.bukkit.event.entity.ProjectileLaunchEvent.class, f -> {
            Entity made = f.entity();
            return made instanceof org.bukkit.entity.Projectile projectile
                ? new org.bukkit.event.entity.ProjectileLaunchEvent(projectile) : null;
        });
        EventBridge.map("vehicle.damage", org.bukkit.event.vehicle.VehicleDamageEvent.class, f -> {
            Entity vehicle = f.entity();
            Entity attacker = f.whole("attacker_id") >= 0 ? BasaltServer.instance().entityById(f.world(),
                (int) f.whole("attacker_id")) : null;
            return vehicle instanceof org.bukkit.entity.Vehicle made
                ? new org.bukkit.event.vehicle.VehicleDamageEvent(made, DamageSource.builder(DamageType.GENERIC).build(),
                    attacker, f.real("amount")) : null;
        }, (e, f) -> f.put("amount", ((org.bukkit.event.vehicle.VehicleDamageEvent) e).getDamage()));
        EventBridge.map("vehicle.destroy", org.bukkit.event.vehicle.VehicleDestroyEvent.class, f -> {
            Entity vehicle = f.entity();
            Entity attacker = f.whole("attacker_id") >= 0 ? BasaltServer.instance().entityById(f.world(),
                (int) f.whole("attacker_id")) : null;
            return vehicle instanceof org.bukkit.entity.Vehicle made
                ? new org.bukkit.event.vehicle.VehicleDestroyEvent(made, DamageSource.builder(DamageType.GENERIC).build(),
                    attacker) : null;
        });
        EventBridge.map("entity.heal", EntityRegainHealthEvent.class, f -> {
            Entity healed = f.entity();
            return healed == null ? null : new EntityRegainHealthEvent(healed, f.real("amount"),
                regainReason(f.text("cause")), f.flag("fast_regen"));
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
        EventBridge.map("player.effect", EntityPotionEffectEvent.class, f -> {
            Player affected = f.player();
            PotionEffectType type = Registry.EFFECT.get(NamespacedKey.fromString(f.text("effect")));
            if (affected == null || type == null) {
                return null;
            }
            PotionEffect old = affected.getPotionEffect(type);
            return new EntityPotionEffectEvent(affected, old,
                new PotionEffect(type, (int) f.whole("duration"), (int) f.whole("amplifier")), null,
                EntityPotionEffectEvent.Cause.UNKNOWN, old == null ? EntityPotionEffectEvent.Action.ADDED
                    : EntityPotionEffectEvent.Action.CHANGED, false);
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
        Entity culprit = f.whole("attacker_id") >= 0 ? BasaltServer.instance().entityById(f.world(),
            (int) f.whole("attacker_id")) : null;
        Entity attacker = f.whole("direct_id") >= 0 ? BasaltServer.instance().entityById(f.world(),
            (int) f.whole("direct_id")) : null;
        if (attacker == null) {
            attacker = culprit;
        }
        if (attacker == null && !f.text("attacker_name").isEmpty()) {
            attacker = BasaltServer.instance().playerByName(f.text("attacker_name"));
        }
        DamageSource.Builder builder = DamageSource.builder(type == null ? DamageType.GENERIC : type);
        if (attacker != null) {
            builder.withCausingEntity(culprit != null ? culprit : attacker).withDirectEntity(attacker);
        }
        DamageSource source = builder.build();
        org.bukkit.block.Block blockSource = attacker == null ? damagingBlock(cause, target) : null;
        Event event = attacker != null ? new EntityDamageByEntityEvent(attacker, target, mapped, source, f.real("amount"))
            : blockSource != null ? new EntityDamageByBlockEvent(blockSource, blockSource.getState(), target, mapped, source,
                f.real("amount")) : new EntityDamageEvent(target, mapped, source, f.real("amount"));
        if (target instanceof Cored cored) {
            cored.core().lastDamageCause = (EntityDamageEvent) event;
        }
        return event;
    }

    private static org.bukkit.block.Block damagingBlock(String cause, Entity target) {
        java.util.Set<org.bukkit.Material> wanted = switch (cause) {
            case "cactus" -> java.util.Set.of(org.bukkit.Material.CACTUS);
            case "sweet_berry_bush" -> java.util.Set.of(org.bukkit.Material.SWEET_BERRY_BUSH);
            case "lava" -> java.util.Set.of(org.bukkit.Material.LAVA);
            case "hot_floor" -> java.util.Set.of(org.bukkit.Material.MAGMA_BLOCK);
            case "in_fire" -> java.util.Set.of(org.bukkit.Material.FIRE, org.bukkit.Material.SOUL_FIRE);
            case "campfire" -> java.util.Set.of(org.bukkit.Material.CAMPFIRE, org.bukkit.Material.SOUL_CAMPFIRE);
            case "stalagmite" -> java.util.Set.of(org.bukkit.Material.POINTED_DRIPSTONE);
            case "bad_respawn_point" -> java.util.Set.of(org.bukkit.Material.RESPAWN_ANCHOR);
            default -> null;
        };
        if (wanted == null) {
            return null;
        }
        org.bukkit.Location at = target.getLocation();
        for (int dy = 0; dy >= -1; dy--) {
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    org.bukkit.block.Block block = at.getWorld().getBlockAt(at.getBlockX() + dx, at.getBlockY() + dy,
                        at.getBlockZ() + dz);
                    if (wanted.contains(block.getType())) {
                        return block;
                    }
                }
            }
        }
        return null;
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
            case "bad_respawn_point" -> EntityDamageEvent.DamageCause.BLOCK_EXPLOSION;
            case "sonic_boom" -> EntityDamageEvent.DamageCause.SONIC_BOOM;
            case "outside_border" -> EntityDamageEvent.DamageCause.WORLD_BORDER;
            default -> EntityDamageEvent.DamageCause.CUSTOM;
        };
    }
}
