package org.basaltmc.bukkit;

/** The world and server events raised from deep in the game systems, as Bukkit and Paper events. */
final class HookWorldEvents {
    private HookWorldEvents() {}

    static void install() {
        EventBridge.map("world.border_bounds", io.papermc.paper.event.world.border.WorldBorderBoundsChangeEvent.class,
            f -> {
                BasaltWorld world = f.world();
                if (world == null) {
                    return null;
                }
                long duration = f.whole("duration_ms");
                return new io.papermc.paper.event.world.border.WorldBorderBoundsChangeEvent(world,
                    world.getWorldBorder(), duration > 0
                        ? io.papermc.paper.event.world.border.WorldBorderBoundsChangeEvent.Type.STARTED_MOVE
                        : io.papermc.paper.event.world.border.WorldBorderBoundsChangeEvent.Type.INSTANT_MOVE,
                    f.real("old_size"), f.real("new_size"), duration);
            });
        EventBridge.map("world.border_center", io.papermc.paper.event.world.border.WorldBorderCenterChangeEvent.class,
            f -> {
                BasaltWorld world = f.world();
                return world == null ? null
                    : new io.papermc.paper.event.world.border.WorldBorderCenterChangeEvent(world,
                        world.getWorldBorder(), new org.bukkit.Location(world, f.real("old_x"), 0, f.real("old_z")),
                        new org.bukkit.Location(world, f.real("new_x"), 0, f.real("new_z")));
            });
        EventBridge.map("world.game_event", org.bukkit.event.world.GenericGameEvent.class, f -> {
            BasaltWorld world = f.world();
            org.bukkit.GameEvent event = org.bukkit.Registry.GAME_EVENT.get(
                org.bukkit.NamespacedKey.minecraft(f.text("event")));
            if (world == null || event == null) {
                return null;
            }
            String uuid = f.text("entity_uuid");
            return new org.bukkit.event.world.GenericGameEvent(event, f.location(),
                uuid.isEmpty() ? null : world.getEntity(java.util.UUID.fromString(uuid)), 16, false);
        });
        EventBridge.map("world.portal_create", org.bukkit.event.world.PortalCreateEvent.class, f -> {
            BasaltWorld world = f.world();
            if (world == null) {
                return null;
            }
            java.util.List<org.bukkit.block.BlockState> states = new java.util.ArrayList<>();
            for (String line : f.text("blocks").split("\n")) {
                String[] at = line.split(",");
                if (at.length == 3) {
                    states.add(world.getBlockAt(Integer.parseInt(at[0]), Integer.parseInt(at[1]),
                        Integer.parseInt(at[2])).getState());
                }
            }
            return new org.bukkit.event.world.PortalCreateEvent(states, world,
                org.bukkit.event.world.PortalCreateEvent.CreateReason.FIRE);
        });
        EventBridge.map("world.unload", org.bukkit.event.world.WorldUnloadEvent.class,
            f -> f.world() == null ? null : new org.bukkit.event.world.WorldUnloadEvent(f.world()));
        EventBridge.map("raid.trigger", org.bukkit.event.raid.RaidTriggerEvent.class, f -> {
            BasaltWorld world = f.world();
            return world == null || f.player() == null ? null : new org.bukkit.event.raid.RaidTriggerEvent(
                new BasaltEndedRaid(f.location(), org.bukkit.Raid.RaidStatus.ONGOING, 0), world, f.player());
        });
        EventBridge.map("raid.wave", org.bukkit.event.raid.RaidSpawnWaveEvent.class, f -> {
            BasaltWorld world = f.world();
            if (world == null) {
                return null;
            }
            java.util.List<org.bukkit.entity.Raider> raiders = new java.util.ArrayList<>();
            for (String id : f.text("raiders").split(",")) {
                if (!id.isEmpty() && world.entity(Integer.parseInt(id)) instanceof org.bukkit.entity.Raider raider) {
                    raiders.add(raider);
                }
            }
            org.bukkit.entity.Entity leader = world.entity((int) f.whole("leader_id"));
            return raiders.isEmpty() ? null : new org.bukkit.event.raid.RaidSpawnWaveEvent(
                new BasaltEndedRaid(f.location(), org.bukkit.Raid.RaidStatus.ONGOING, (int) f.whole("wave")), world,
                leader instanceof org.bukkit.entity.Raider lead ? lead : raiders.get(0), raiders);
        });
    }
}
