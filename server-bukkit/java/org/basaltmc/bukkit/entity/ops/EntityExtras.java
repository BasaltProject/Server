package org.basaltmc.bukkit.entity.ops;

import io.papermc.paper.entity.RemovalReason;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason;
import org.bukkit.event.entity.EntityRemoveEvent;
import org.bukkit.permissions.PermissibleBase;
import org.bukkit.permissions.PermissionAttachment;
import org.bukkit.permissions.PermissionAttachmentInfo;
import org.bukkit.permissions.ServerOperator;

/** Entity facts every entity answers the same way, kept out of the ops interface that exposes them. */
final class EntityExtras {
    private static final ServerOperator kNeverOp = new ServerOperator() {
        @Override
        public boolean isOp() {
            return false;
        }

        @Override
        public void setOp(boolean value) {
            throw new UnsupportedOperationException("Cannot change the operator status of an entity");
        }
    };

    private EntityExtras() {}

    static PermissibleBase permissions(EntityCore core) {
        PermissibleBase held = core.permissions;
        if (held == null) {
            synchronized (core) {
                held = core.permissions;
                if (held == null) {
                    held = new PermissibleBase(kNeverOp);
                    core.permissions = held;
                }
            }
        }
        return held;
    }

    static Set<PermissionAttachmentInfo> effective(EntityCore core) {
        return permissions(core).getEffectivePermissions();
    }

    static PermissionAttachment attach(EntityCore core, org.bukkit.plugin.Plugin plugin) {
        return permissions(core).addAttachment(plugin);
    }

    static Entity peer(Cored holder, int id) {
        return holder.core().entity(id);
    }

    static boolean allied(Entity self, EntityCore core, Entity other) {
        for (java.util.function.Predicate<Entity> rule : core.alliedRules.values()) {
            if (rule.test(other)) {
                return true;
            }
        }
        return false;
    }

    static float airDrag(Entity entity) {
        return switch (entity.getType()) {
            case EXPERIENCE_ORB -> 0.98F;
            case MINECART, CHEST_MINECART, FURNACE_MINECART, HOPPER_MINECART, TNT_MINECART, COMMAND_BLOCK_MINECART,
                SPAWNER_MINECART -> 0.95F;
            case SNOWBALL, EGG, ENDER_PEARL, EXPERIENCE_BOTTLE, SPLASH_POTION, LINGERING_POTION, ARROW,
                SPECTRAL_ARROW, TRIDENT, LLAMA_SPIT -> 0.99F;
            default -> {
                if (entity instanceof org.bukkit.entity.Boat) {
                    yield 1.0F;
                }
                if (entity instanceof org.bukkit.entity.LivingEntity living) {
                    boolean flier = living instanceof org.bukkit.entity.Bee || living instanceof org.bukkit.entity.Parrot;
                    org.bukkit.attribute.AttributeInstance modifier = living
                        .getAttribute(org.bukkit.attribute.Attribute.AIR_DRAG_MODIFIER);
                    float scale = modifier == null ? 1.0F : (float) modifier.getValue();
                    yield Math.max(0.0F, Math.min(1.0F, 1.0F - (1.0F - (flier ? 0.91F : 0.98F)) * scale));
                }
                yield 0.98F;
            }
        };
    }

    static double defaultGravity(Entity entity) {
        if (entity instanceof org.bukkit.entity.LivingEntity living) {
            org.bukkit.attribute.AttributeInstance gravity = living.getAttribute(org.bukkit.attribute.Attribute.GRAVITY);
            return gravity == null ? 0.08 : gravity.getValue();
        }
        return switch (entity.getType()) {
            case ARROW, SPECTRAL_ARROW, TRIDENT, SPLASH_POTION, LINGERING_POTION -> 0.05;
            case EXPERIENCE_BOTTLE -> 0.07;
            case SNOWBALL, EGG, ENDER_PEARL, FISHING_BOBBER, EXPERIENCE_ORB -> 0.03;
            case LLAMA_SPIT -> 0.06;
            case ITEM, FALLING_BLOCK, TNT, SHULKER_BULLET, MINECART, CHEST_MINECART, FURNACE_MINECART,
                HOPPER_MINECART, TNT_MINECART, COMMAND_BLOCK_MINECART, SPAWNER_MINECART -> 0.04;
            default -> entity instanceof org.bukkit.entity.Boat ? 0.04 : 0.0;
        };
    }

    static SpawnReason spawnReason(EntityCore core) {
        String reason = core.text("spawn_reason");
        if (reason == null || reason.isEmpty()) {
            return SpawnReason.DEFAULT;
        }
        try {
            return SpawnReason.valueOf(reason.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return switch (reason) {
                case "event" -> SpawnReason.RAID;
                case "plugin" -> SpawnReason.CUSTOM;
                case "triggered" -> SpawnReason.TRAP;
                case "patrol" -> SpawnReason.PATROL;
                default -> SpawnReason.DEFAULT;
            };
        }
    }

    static RemovalReason removalReason(EntityCore core) {
        if (core.alive()) {
            return null;
        }
        return core.removeCause == EntityRemoveEvent.Cause.DEATH || core.real("health") <= 0.0
            && core.type.isAlive() ? RemovalReason.KILLED : RemovalReason.DISCARDED;
    }

    static EntityRemoveEvent.Cause removeCause(EntityCore core) {
        if (core.alive()) {
            return null;
        }
        if (core.removeCause != null) {
            return core.removeCause;
        }
        return core.type.isAlive() && core.real("health") <= 0.0 ? EntityRemoveEvent.Cause.DEATH
            : EntityRemoveEvent.Cause.DESPAWN;
    }

    static Set<Player> trackers(Entity entity, EntityCore core) {
        Set<Player> out = new java.util.LinkedHashSet<>();
        if (!(core.call("tracked_by") instanceof String[] ids)) {
            return out;
        }
        for (String id : ids) {
            Player player = org.bukkit.Bukkit.getPlayer(java.util.UUID.fromString(id));
            if (player != null && player != entity) {
                out.add(player);
            }
        }
        return out;
    }

    static void hurtAnimation(EntityCore core, float yaw, Collection<Player> viewers) {
        List<Object> arguments = new ArrayList<>();
        arguments.add((double) yaw);
        for (Player viewer : viewers) {
            arguments.add(viewer.getUniqueId().toString());
        }
        if (arguments.size() > 1) {
            core.call("hurt_animation", arguments.toArray());
        }
    }

    static void lookAt(EntityCore core, Entity self, double x, double y, double z,
            io.papermc.paper.entity.LookAnchor anchor) {
        Location from = self.getLocation();
        if (anchor == io.papermc.paper.entity.LookAnchor.EYES && self instanceof org.bukkit.entity.LivingEntity living) {
            from = living.getEyeLocation();
        }
        double dx = x - from.getX();
        double dy = y - from.getY();
        double dz = z - from.getZ();
        double flat = Math.sqrt(dx * dx + dz * dz);
        float yaw = (float) (Math.toDegrees(Math.atan2(dz, dx)) - 90.0);
        float pitch = (float) -Math.toDegrees(Math.atan2(dy, flat));
        self.setRotation(yaw, pitch);
    }

    static Component teamDisplayName(Entity entity) {
        org.bukkit.scoreboard.Team team = org.bukkit.Bukkit.getScoreboardManager().getMainScoreboard()
            .getEntityTeam(entity);
        Component name = entity.name();
        if (team == null) {
            return name;
        }
        Component colored = team.hasColor() ? name.colorIfAbsent(team.color()) : name;
        return Component.empty().append(team.prefix()).append(colored).append(team.suffix());
    }

    static void playEffect(EntityCore core, Entity self, org.bukkit.EntityEffect effect) {
        if (effect.isApplicableTo(self)) {
            core.call("entity_event", (long) effect.getData());
        }
    }

    static Entity.Spigot spigot() {
        return new Entity.Spigot() {
            @Override
            public void sendMessage(net.md_5.bungee.api.chat.BaseComponent component) {}

            @Override
            public void sendMessage(net.md_5.bungee.api.chat.BaseComponent... components) {}

            @Override
            public void sendMessage(java.util.UUID sender, net.md_5.bungee.api.chat.BaseComponent component) {}

            @Override
            public void sendMessage(java.util.UUID sender, net.md_5.bungee.api.chat.BaseComponent... components) {}
        };
    }

    static BasaltServer server() {
        return BasaltServer.instance();
    }
}
