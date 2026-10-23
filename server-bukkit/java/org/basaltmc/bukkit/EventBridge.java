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

        org.bukkit.entity.Entity entity() {
            return BasaltServer.instance().entityById(world(), (int) whole("entity_id"));
        }

        org.bukkit.block.Block block() {
            return world().getBlockAt((int) whole("x"), (int) whole("y"), (int) whole("z"));
        }

        org.bukkit.inventory.ItemStack stack(String field) {
            String id = text(field);
            if (id.isEmpty()) {
                return null;
            }
            org.bukkit.Material material = org.bukkit.Material.matchMaterial(id);
            if (material == null || material.isAir()) {
                return null;
            }
            Player who = player();
            if (who != null) {
                for (org.bukkit.inventory.ItemStack held : new org.bukkit.inventory.ItemStack[] {
                    who.getInventory().getItemInMainHand(), who.getInventory().getItemInOffHand()}) {
                    if (held != null && held.getType() == material && held.getAmount() > 0) {
                        return held.clone();
                    }
                }
            }
            return new org.bukkit.inventory.ItemStack(material, 1);
        }

        Location location() {
            return new Location(world(), real("x"), real("y"), real("z"));
        }
    }

    private record Mapping(String basalt, Class<? extends Event> produced, Function<Fields, Event> build,
            BiConsumer<Event, Fields> writeBack, BiConsumer<Event, Fields> refresh) {}

    private static final List<Mapping> mappings = new java.util.ArrayList<>();
    private static final Map<String, List<Mapping>> byName = new HashMap<>();
    private static final Set<String> subscribed = new HashSet<>();
    private static final Map<String, Event> inFlight = new HashMap<>();
    private static final Set<String> failed = new HashSet<>();
    private static final Map<String, Long> generations = new HashMap<>();
    private static final Map<String, Map<String, Integer>> indexes = new HashMap<>();

    private EventBridge() {}

    static void map(String basalt, Class<? extends Event> produced, Function<Fields, Event> build) {
        map(basalt, produced, build, null);
    }

    static void map(String basalt, Class<? extends Event> produced, Function<Fields, Event> build,
            BiConsumer<Event, Fields> writeBack) {
        map(basalt, produced, build, writeBack, null);
    }

    static void map(String basalt, Class<? extends Event> produced, Function<Fields, Event> build,
            BiConsumer<Event, Fields> writeBack, BiConsumer<Event, Fields> refresh) {
        Mapping mapping = new Mapping(basalt, produced, build, writeBack, refresh);
        mappings.add(mapping);
        byName.computeIfAbsent(basalt, key -> new java.util.ArrayList<>()).add(mapping);
    }

    static void mapAlways(String basalt, java.util.function.Consumer<Fields> effect) {
        map(basalt, org.bukkit.event.server.ServerEvent.class, f -> {
            effect.accept(f);
            return null;
        });
        Natives.subscribe(basalt, EventPriority.MONITOR.ordinal());
    }

    static {
        InventoryEvents.register();
        PlayerEvents.install();
        BlockEvents.install();
        WorldEvents.install();
        ServerEvents.install();
        map("player.join", PlayerJoinEvent.class, f -> new PlayerJoinEvent(f.player(),
            Component.translatable("multiplayer.player.joined", Component.text(f.text("player_name")))));
        map("player.quit", PlayerQuitEvent.class, f -> new PlayerQuitEvent(f.player(),
            Component.translatable("multiplayer.player.left", Component.text(f.text("player_name"))),
            PlayerQuitEvent.QuitReason.DISCONNECTED));
        map("player.chat", AsyncPlayerChatEvent.class,
            f -> new AsyncPlayerChatEvent(true, f.player(), f.text("message"), new HashSet<>(
                BasaltServer.instance().getOnlinePlayers())),
            (e, f) -> f.put("message", ((AsyncPlayerChatEvent) e).getMessage()),
            (e, f) -> ((AsyncPlayerChatEvent) e).setMessage(f.text("message")));
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
                new Location(world, f.real("x"), f.real("y"), f.real("z")), PlayerEvents.teleportCause(f.text("cause")));
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
        map("player.interact", org.bukkit.event.player.PlayerInteractEvent.class, f -> {
            org.bukkit.event.block.Action action = f.text("action").startsWith("left")
                ? org.bukkit.event.block.Action.LEFT_CLICK_BLOCK : org.bukkit.event.block.Action.RIGHT_CLICK_BLOCK;
            return new org.bukkit.event.player.PlayerInteractEvent(f.player(), action, f.stack("item"), f.block(),
                org.bukkit.block.BlockFace.SELF, org.bukkit.inventory.EquipmentSlot.HAND);
        });
        map("block.damage", org.bukkit.event.player.PlayerInteractEvent.class,
            f -> new org.bukkit.event.player.PlayerInteractEvent(f.player(),
                org.bukkit.event.block.Action.LEFT_CLICK_BLOCK, f.stack("item"), f.block(),
                org.bukkit.block.BlockFace.SELF, org.bukkit.inventory.EquipmentSlot.HAND));
        map("block.damage", org.bukkit.event.block.BlockDamageEvent.class, f -> new org.bukkit.event.block.BlockDamageEvent(
            f.player(), f.block(), f.player().getInventory().getItemInMainHand(), false));
        map("block.place", org.bukkit.event.block.BlockPlaceEvent.class, f -> {
            org.bukkit.block.Block placed = f.block();
            return new org.bukkit.event.block.BlockPlaceEvent(placed, placed.getState(), placed.getRelative(
                org.bukkit.block.BlockFace.DOWN), f.stack("block"), f.player(), true,
                org.bukkit.inventory.EquipmentSlot.HAND);
        });
        map("player.respawn", org.bukkit.event.player.PlayerRespawnEvent.class,
            f -> new org.bukkit.event.player.PlayerRespawnEvent(f.player(), f.location(), false));
        map("player.world_change", org.bukkit.event.player.PlayerChangedWorldEvent.class,
            f -> new org.bukkit.event.player.PlayerChangedWorldEvent(f.player(),
                BasaltServer.instance().world(f.text("from_world"))));
        map("player.item_consume", org.bukkit.event.player.PlayerItemConsumeEvent.class,
            f -> new org.bukkit.event.player.PlayerItemConsumeEvent(f.player(), f.stack("item")));
        map("player.swap_hands", org.bukkit.event.player.PlayerSwapHandItemsEvent.class,
            f -> new org.bukkit.event.player.PlayerSwapHandItemsEvent(f.player(), f.stack("main_item"),
                f.stack("off_item")));
        map("player.food_change", org.bukkit.event.entity.FoodLevelChangeEvent.class,
            f -> new org.bukkit.event.entity.FoodLevelChangeEvent(f.player(), (int) f.whole("food")),
            (e, f) -> f.put("food", (long) ((org.bukkit.event.entity.FoodLevelChangeEvent) e).getFoodLevel()));
        map("player.level_change", org.bukkit.event.player.PlayerLevelChangeEvent.class,
            f -> new org.bukkit.event.player.PlayerLevelChangeEvent(f.player(), (int) f.whole("from_level"),
                (int) f.whole("level")));
        map("player.interact_entity", org.bukkit.event.player.PlayerInteractEntityEvent.class, f -> {
            org.bukkit.entity.Entity target = f.entity();
            return target == null || f.text("action").equals("attack") ? null
                : new org.bukkit.event.player.PlayerInteractEntityEvent(f.player(), target);
        });
        map("world.chunk_load", org.bukkit.event.world.ChunkLoadEvent.class,
            f -> new org.bukkit.event.world.ChunkLoadEvent(
                f.world().getChunkAt((int) f.whole("chunk_x"), (int) f.whole("chunk_z"), false), f.flag("generated")));
        map("world.chunk_unload", org.bukkit.event.world.ChunkUnloadEvent.class,
            f -> new org.bukkit.event.world.ChunkUnloadEvent(
                f.world().getChunkAt((int) f.whole("chunk_x"), (int) f.whole("chunk_z"), false), true));
        map("world.weather_change", org.bukkit.event.weather.WeatherChangeEvent.class,
            f -> f.flag("raining") == f.flag("from_raining") ? null
                : new org.bukkit.event.weather.WeatherChangeEvent(f.world(), f.flag("raining")));
        map("world.weather_change", org.bukkit.event.weather.ThunderChangeEvent.class,
            f -> f.flag("thundering") == f.flag("from_thundering") ? null
                : new org.bukkit.event.weather.ThunderChangeEvent(f.world(), f.flag("thundering")));
        EntityEvents.install();
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
        List<Mapping> found = byName.get(name);
        if (found == null) {
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
        boolean result = cancelled;
        for (Mapping mapping : found) {
            String key = name + "#" + mapping.produced.getName();
            try {
                Event event = inFlight.get(key);
                if (event == null || generations.getOrDefault(key, -1L) != generation) {
                    event = mapping.build.apply(fields);
                    inFlight.put(key, event);
                    generations.put(key, generation);
                }
                if (event == null) {
                    continue;
                }
                if (mapping.refresh != null) {
                    mapping.refresh.accept(event, fields);
                }
                if (event instanceof Cancellable cancellable) {
                    cancellable.setCancelled(result);
                }
                BasaltServer.instance().pluginManagerImpl().callAt(event, EventPriority.values()[priority]);
                if (mapping.writeBack != null) {
                    mapping.writeBack.accept(event, fields);
                }
                result = event instanceof Cancellable cancellable && cancellable.isCancelled();
            } catch (Throwable failure) {
                inFlight.remove(key);
                if (failed.add(key)) {
                    BasaltServer.instance().getLogger().log(java.util.logging.Level.SEVERE,
                        "Could not build " + mapping.produced.getSimpleName() + " from " + name, failure);
                }
            }
        }
        return result;
    }
}
