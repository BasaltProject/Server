package org.basaltmc.bukkit;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.function.BiConsumer;
import java.util.function.Function;
import net.kyori.adventure.text.Component;
import org.bukkit.GameMode;
import org.bukkit.Location;
import org.bukkit.entity.Player;
import org.bukkit.event.Cancellable;
import org.bukkit.event.Event;
import org.bukkit.event.EventPriority;
import org.bukkit.event.block.BlockBreakEvent;
import org.bukkit.event.player.AsyncPlayerChatEvent;
import org.bukkit.event.player.PlayerCommandPreprocessEvent;
import org.bukkit.event.player.PlayerGameModeChangeEvent;
import org.bukkit.event.player.PlayerItemHeldEvent;
import org.bukkit.event.player.PlayerJoinEvent;
import org.bukkit.event.player.PlayerMoveEvent;
import org.bukkit.event.player.PlayerQuitEvent;
import org.bukkit.event.player.PlayerTeleportEvent;
import org.bukkit.event.player.PlayerToggleSneakEvent;
import org.bukkit.event.player.PlayerToggleSprintEvent;
import org.bukkit.event.server.ServerCommandEvent;
import org.bukkit.event.server.ServerLoadEvent;
import org.bukkit.event.world.WorldLoadEvent;
import org.bukkit.event.world.WorldSaveEvent;

final class EventBridge {
    static final class Fields {
        private final Map<String, Integer> index;
        private final Object[] values;

        Fields(Map<String, Integer> index, Object[] values) {
            this.index = index;
            this.values = values;
        }

        String text(String name) {
            Object value = get(name);
            return value == null ? "" : value.toString();
        }

        double real(String name) {
            return get(name) instanceof Number number ? number.doubleValue() : 0.0;
        }

        long whole(String name) {
            return get(name) instanceof Number number ? number.longValue() : 0L;
        }

        boolean flag(String name) {
            return get(name) instanceof Boolean value && value;
        }

        Object get(String name) {
            Integer at = index.get(name);
            return at == null ? null : values[at];
        }

        void put(String name, Object value) {
            Integer at = index.get(name);
            if (at != null) {
                values[at] = value;
            }
        }

        Player player() {
            String id = text("player_uuid");
            return id.isEmpty() ? null : BasaltServer.instance().player(UUID.fromString(id), text("player_name"));
        }

        BasaltWorld world() {
            return BasaltServer.instance().world(text("world"));
        }
    }

    private record Mapping(String basalt, Class<? extends Event> produced, Function<Fields, Event> build,
            BiConsumer<Event, Fields> writeBack, boolean needsPlayer) {}

    private static final List<Mapping> mappings = new java.util.ArrayList<>();
    private static final Map<String, Mapping> byName = new HashMap<>();
    private static final Set<String> subscribed = new HashSet<>();
    private static final Map<String, Event> inFlight = new HashMap<>();
    private static final Map<String, Long> generations = new HashMap<>();
    private static final Map<String, Map<String, Integer>> indexes = new HashMap<>();

    private EventBridge() {}

    private static void map(String basalt, Class<? extends Event> produced, Function<Fields, Event> build) {
        map(basalt, produced, build, null);
    }

    private static void map(String basalt, Class<? extends Event> produced, Function<Fields, Event> build,
            BiConsumer<Event, Fields> writeBack) {
        Mapping mapping = new Mapping(basalt, produced, build, writeBack, false);
        mappings.add(mapping);
        byName.put(basalt, mapping);
    }

    static {
        map("player.join", PlayerJoinEvent.class, f -> new PlayerJoinEvent(f.player(),
            Component.translatable("multiplayer.player.joined", Component.text(f.text("player_name")))));
        map("player.quit", PlayerQuitEvent.class, f -> new PlayerQuitEvent(f.player(),
            Component.translatable("multiplayer.player.left", Component.text(f.text("player_name"))),
            PlayerQuitEvent.QuitReason.DISCONNECTED));
        map("player.chat", AsyncPlayerChatEvent.class,
            f -> new AsyncPlayerChatEvent(false, f.player(), f.text("message"), new HashSet<>(
                BasaltServer.instance().getOnlinePlayers())),
            (e, f) -> f.put("message", ((AsyncPlayerChatEvent) e).getMessage()));
        map("player.command", PlayerCommandPreprocessEvent.class,
            f -> new PlayerCommandPreprocessEvent(f.player(), "/" + f.text("command")),
            (e, f) -> {
                String message = ((PlayerCommandPreprocessEvent) e).getMessage();
                f.put("command", message.startsWith("/") ? message.substring(1) : message);
            });
        map("player.move", PlayerMoveEvent.class, f -> {
            BasaltWorld world = f.world();
            return new PlayerMoveEvent(f.player(), new Location(world, f.real("from_x"), f.real("from_y"), f.real("from_z")),
                new Location(world, f.real("x"), f.real("y"), f.real("z")));
        });
        map("player.teleport", PlayerTeleportEvent.class, f -> {
            BasaltWorld world = f.world();
            return new PlayerTeleportEvent(f.player(),
                new Location(world, f.real("from_x"), f.real("from_y"), f.real("from_z")),
                new Location(world, f.real("x"), f.real("y"), f.real("z")), PlayerTeleportEvent.TeleportCause.UNKNOWN);
        });
        map("player.gamemode_change", PlayerGameModeChangeEvent.class, f -> new PlayerGameModeChangeEvent(
            f.player(), gameMode(f.text("gamemode")), PlayerGameModeChangeEvent.Cause.UNKNOWN, null));
        map("player.sneak", PlayerToggleSneakEvent.class,
            f -> new PlayerToggleSneakEvent(f.player(), f.flag("sneaking")));
        map("player.sprint", PlayerToggleSprintEvent.class,
            f -> new PlayerToggleSprintEvent(f.player(), f.flag("sprinting")));
        map("player.item_held", PlayerItemHeldEvent.class,
            f -> new PlayerItemHeldEvent(f.player(), (int) f.whole("from_slot"), (int) f.whole("slot")));
        map("block.break", BlockBreakEvent.class, f -> new BlockBreakEvent(
            f.world().getBlockAt((int) f.whole("x"), (int) f.whole("y"), (int) f.whole("z")), f.player()));
        map("world.load", WorldLoadEvent.class, f -> new WorldLoadEvent(BasaltServer.instance().world(f.text("world"))));
        map("world.save", WorldSaveEvent.class, f -> new WorldSaveEvent(BasaltServer.instance().world(f.text("world"))));
        map("server.load", ServerLoadEvent.class, f -> new ServerLoadEvent(ServerLoadEvent.LoadType.STARTUP));
        map("server.command", ServerCommandEvent.class,
            f -> new ServerCommandEvent(BasaltServer.instance().consoleImpl(), f.text("command")),
            (e, f) -> f.put("command", ((ServerCommandEvent) e).getCommand()));
    }

    private static GameMode gameMode(String name) {
        try {
            return GameMode.valueOf(name.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return GameMode.SURVIVAL;
        }
    }

    static void listening(Class<? extends Event> registered, EventPriority priority) {
        for (Mapping mapping : mappings) {
            if (!registered.isAssignableFrom(mapping.produced)) {
                continue;
            }
            String key = mapping.basalt + "@" + priority.ordinal();
            if (subscribed.add(key)) {
                Natives.subscribe(mapping.basalt, priority.ordinal());
            }
        }
    }

    static boolean fire(String name, int priority, long generation, Object[] values, boolean cancelled) {
        Mapping mapping = byName.get(name);
        if (mapping == null) {
            return cancelled;
        }
        Map<String, Integer> index = indexes.computeIfAbsent(name, key -> {
            String[] fieldNames = Natives.eventFields(key);
            Map<String, Integer> built = new HashMap<>();
            for (int at = 0; at < fieldNames.length; at++) {
                built.put(fieldNames[at], at);
            }
            return built;
        });
        Fields fields = new Fields(index, values);
        Event event = inFlight.get(name);
        if (event == null || generations.getOrDefault(name, -1L) != generation) {
            event = mapping.build.apply(fields);
            inFlight.put(name, event);
            generations.put(name, generation);
        }
        if (event instanceof Cancellable cancellable) {
            cancellable.setCancelled(cancelled);
        }
        BasaltServer.instance().pluginManagerImpl().callAt(event, EventPriority.values()[priority]);
        if (mapping.writeBack != null) {
            mapping.writeBack.accept(event, fields);
        }
        return event instanceof Cancellable cancellable && cancellable.isCancelled();
    }
}
