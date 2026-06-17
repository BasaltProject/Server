package org.basaltmc.bukkit;

import org.bukkit.GameRule;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.event.world.ClockTimeSkipEvent;

/** The world events Basalt raises, as the Bukkit and Paper events plugins listen for. */
final class WorldEvents {
    private WorldEvents() {}

    static void install() {
        EventBridge.map("world.load", org.bukkit.event.world.WorldInitEvent.class, f -> BasaltServer.instance()
            .announced() ? new org.bukkit.event.world.WorldInitEvent(f.world()) : null);
        EventBridge.map("world.lightning", org.bukkit.event.weather.LightningStrikeEvent.class, f -> {
            BasaltWorld world = f.world();
            if (world == null) {
                return null;
            }
            Entity bolt = org.basaltmc.bukkit.entity.EntityCore.template(world, EntityType.LIGHTNING_BOLT, f.real("x"),
                f.real("y"), f.real("z"), 0f, 0f);
            return bolt instanceof org.bukkit.entity.LightningStrike strike
                ? new org.bukkit.event.weather.LightningStrikeEvent(world, strike,
                    org.bukkit.event.weather.LightningStrikeEvent.Cause.WEATHER) : null;
        });
        EventBridge.map("world.time_skip", org.bukkit.event.world.TimeSkipEvent.class, f -> {
            ClockTimeSkipEvent.SkipReason reason = switch (f.text("cause")) {
                case "command" -> ClockTimeSkipEvent.SkipReason.COMMAND;
                case "sleep", "night_skip" -> ClockTimeSkipEvent.SkipReason.NIGHT_SKIP;
                default -> ClockTimeSkipEvent.SkipReason.CUSTOM;
            };
            return f.world() == null ? null : new org.bukkit.event.world.TimeSkipEvent(f.world(), reason,
                f.whole("time") - f.whole("from_time"));
        });


        EventBridge.map("raid.finish", org.bukkit.event.raid.RaidFinishEvent.class, f -> {
            if (!f.flag("won") || f.world() == null) {
                return null;
            }
            org.bukkit.Location at = new org.bukkit.Location(f.world(), f.real("x"), f.real("y"), f.real("z"));
            return new org.bukkit.event.raid.RaidFinishEvent(new BasaltRaid(at, org.bukkit.Raid.RaidStatus.VICTORY,
                (int) f.whole("waves")), f.world(), new java.util.ArrayList<>());
        });
        EventBridge.map("raid.finish", org.bukkit.event.raid.RaidStopEvent.class, f -> {
            if (f.flag("won") || f.world() == null) {
                return null;
            }
            org.bukkit.Location at = new org.bukkit.Location(f.world(), f.real("x"), f.real("y"), f.real("z"));
            return new org.bukkit.event.raid.RaidStopEvent(new BasaltRaid(at, org.bukkit.Raid.RaidStatus.LOSS,
                (int) f.whole("waves")), f.world(), org.bukkit.event.raid.RaidStopEvent.Reason.FINISHED);
        });
        EventBridge.map("world.chunk_load", org.bukkit.event.world.ChunkPopulateEvent.class, f -> !f.flag("generated")
            ? null : new org.bukkit.event.world.ChunkPopulateEvent(
                f.world().getChunkAt((int) f.whole("chunk_x"), (int) f.whole("chunk_z"), false)));
        EventBridge.map("world.structure_grow", org.bukkit.event.world.StructureGrowEvent.class, f -> {
            BasaltWorld world = f.world();
            String species = f.text("species");
            org.bukkit.TreeType type = species.contains("birch") ? org.bukkit.TreeType.BIRCH
                : species.contains("spruce") ? org.bukkit.TreeType.REDWOOD
                : species.contains("jungle") ? org.bukkit.TreeType.JUNGLE
                : species.contains("acacia") ? org.bukkit.TreeType.ACACIA
                : species.contains("dark_oak") ? org.bukkit.TreeType.DARK_OAK
                : species.contains("cherry") ? org.bukkit.TreeType.CHERRY
                : species.contains("mangrove") ? org.bukkit.TreeType.MANGROVE : org.bukkit.TreeType.TREE;
            return world == null ? null : new org.bukkit.event.world.StructureGrowEvent(f.location(), type, false, null,
                new java.util.ArrayList<>());
        });
        EventBridge.map("world.spawn_change", org.bukkit.event.world.SpawnChangeEvent.class, f -> f.world() == null ? null
            : new org.bukkit.event.world.SpawnChangeEvent(f.world(), new org.bukkit.Location(f.world(),
                f.real("from_x"), f.real("from_y"), f.real("from_z"))));

        EventBridge.map("world.chunk_load", org.bukkit.event.world.EntitiesLoadEvent.class, f -> {
            org.bukkit.Chunk chunk = f.world().getChunkAt((int) f.whole("chunk_x"), (int) f.whole("chunk_z"), false);
            return new org.bukkit.event.world.EntitiesLoadEvent(chunk, java.util.List.of(chunk.getEntities()));
        });
        EventBridge.map("world.chunk_unload", org.bukkit.event.world.EntitiesUnloadEvent.class, f -> {
            org.bukkit.Chunk chunk = f.world().getChunkAt((int) f.whole("chunk_x"), (int) f.whole("chunk_z"), false);
            return new org.bukkit.event.world.EntitiesUnloadEvent(chunk, java.util.List.of(chunk.getEntities()));
        });
        EventBridge.map("world.game_rule_change", io.papermc.paper.event.world.WorldGameRuleChangeEvent.class, f -> {
            GameRule<?> rule;
            try {
                rule = GameRule.getByName(f.text("rule"));
            } catch (Throwable unavailable) {
                return null;
            }
            Object sender = f.text("player_name").isEmpty() ? BasaltServer.instance().consoleImpl()
                : BasaltServer.instance().playerByName(f.text("player_name"));
            return rule == null || f.world() == null ? null : new io.papermc.paper.event.world.WorldGameRuleChangeEvent(
                f.world(), sender instanceof org.bukkit.command.CommandSender who ? who
                    : BasaltServer.instance().consoleImpl(), rule, f.text("value"));
        });
    }
}
