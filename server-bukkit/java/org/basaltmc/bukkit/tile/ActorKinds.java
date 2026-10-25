package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.block.BlockState;
import org.bukkit.block.Beehive;
import org.bukkit.block.BrushableBlock;
import org.bukkit.block.CalibratedSculkSensor;
import org.bukkit.block.CreakingHeart;
import org.bukkit.block.SculkCatalyst;
import org.bukkit.block.SculkSensor;
import org.bukkit.block.SculkShrieker;
import org.bukkit.block.SuspiciousSand;
import org.bukkit.block.TrialSpawner;
import org.bukkit.block.Vault;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.loot.LootTable;
import org.bukkit.spawner.TrialSpawnerConfiguration;

final class ActorKinds {
    private ActorKinds() {}

    private static Set<UUID> uuids(Object raw) {
        Set<UUID> out = new LinkedHashSet<>();
        if (raw instanceof List<?> list) {
            for (Object value : list) {
                UUID id = Convert.uuidOf(value);
                if (id != null) {
                    out.add(id);
                }
            }
        }
        return out;
    }

    private static List<Object> ints(Set<UUID> ids) {
        List<Object> out = new ArrayList<>();
        for (UUID id : ids) {
            out.add(Convert.uuidToInts(id));
        }
        return out;
    }

    static final class SculkKind implements TileKind {
        private final Class<? extends BlockState> face;
        private final int range;

        SculkKind(Class<? extends BlockState> face, int range) {
            this.face = face;
            this.range = range;
        }

        @Override
        public Class<? extends BlockState> face() {
            return face;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getLastVibrationFrequency":
                    return tile.integer("last_vibration_frequency", 0);
                case "setLastVibrationFrequency":
                    tile.put("last_vibration_frequency", (Integer) args[0]);
                    return null;
                case "getListenerRange":
                    return tile.integer("Paper.ListenerRange", range);
                case "setListenerRange":
                    if ((Integer) args[0] <= 0) {
                        throw new IllegalArgumentException("Vibration listener range must be greater than 0");
                    }
                    tile.put("Paper.ListenerRange", args[0]);
                    return null;
                case "getWarningLevel":
                    return tile.integer("warning_level", 0);
                case "setWarningLevel":
                    tile.put("warning_level", (Integer) args[0]);
                    return null;
                case "tryShriek":
                    if (args[0] != null) {
                        WorldEnvNatives.sculk(tile.world.getName(), "shriek", tile.x, tile.y, tile.z, 0, 0, 0, 0,
                            ((Player) args[0]).getUniqueId().toString());
                    }
                    return null;
                case "bloom": {
                    int charge = (Integer) args[1];
                    if (charge <= 0) {
                        throw new IllegalArgumentException("charge must be positive");
                    }
                    int[] at = args[0] instanceof org.bukkit.block.Block block
                        ? new int[] {block.getX(), block.getY(), block.getZ()}
                        : new int[] {((io.papermc.paper.math.Position) args[0]).blockX(),
                            ((io.papermc.paper.math.Position) args[0]).blockY(),
                            ((io.papermc.paper.math.Position) args[0]).blockZ()};
                    WorldEnvNatives.sculk(tile.world.getName(), "bloom", tile.x, tile.y, tile.z, at[0], at[1], at[2],
                        charge, null);
                    return null;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class VaultKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Vault.class;
        }

        private static LootTable table(Object id) {
            NamespacedKey key = id instanceof String text ? NamespacedKey.fromString(text) : null;
            return key == null ? null : Bukkit.getLootTable(key);
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            Map<String, Object> config = tile.mapOrCreate("config");
            Map<String, Object> server = tile.mapOrCreate("server_data");
            Map<String, Object> shared = tile.mapOrCreate("shared_data");
            switch (name) {
                case "getActivationRange":
                    return Nbt.real(config, "activation_range", 4.0);
                case "setActivationRange":
                    config.put("activation_range", (Double) args[0]);
                    return null;
                case "getDeactivationRange":
                    return Nbt.real(config, "deactivation_range", 4.5);
                case "setDeactivationRange":
                    config.put("deactivation_range", (Double) args[0]);
                    return null;
                case "getKeyItem": {
                    Map<String, Object> key = Nbt.map(config, "key_item");
                    return key == null ? new ItemStack(org.bukkit.Material.TRIAL_KEY) : Convert.stack(key);
                }
                case "setKeyItem":
                    config.put("key_item", Convert.stack((ItemStack) args[0]));
                    return null;
                case "getLootTable":
                    return table(config.get("loot_table"));
                case "setLootTable":
                    config.put("loot_table", ((LootTable) args[0]).getKey().toString());
                    return null;
                case "getDisplayedLootTable":
                    return table(config.get("override_loot_table_to_display"));
                case "setDisplayedLootTable":
                    if (args[0] == null) {
                        config.remove("override_loot_table_to_display");
                    } else {
                        config.put("override_loot_table_to_display", ((LootTable) args[0]).getKey().toString());
                    }
                    return null;
                case "getNextStateUpdateTime":
                    return Nbt.longValue(server, "state_updating_resumes_at", 0L);
                case "setNextStateUpdateTime":
                    server.put("state_updating_resumes_at", (Long) args[0]);
                    return null;
                case "getRewardedPlayers":
                    return uuids(server.get("rewarded_players"));
                case "addRewardedPlayer": {
                    Set<UUID> ids = uuids(server.get("rewarded_players"));
                    boolean added = ids.add((UUID) args[0]);
                    server.put("rewarded_players", ints(ids));
                    return added;
                }
                case "removeRewardedPlayer": {
                    Set<UUID> ids = uuids(server.get("rewarded_players"));
                    boolean removed = ids.remove((UUID) args[0]);
                    server.put("rewarded_players", ints(ids));
                    return removed;
                }
                case "hasRewardedPlayer":
                    return uuids(server.get("rewarded_players")).contains((UUID) args[0]);
                case "getConnectedPlayers":
                    return new HashSet<>(uuids(shared.get("connected_players")));
                case "hasConnectedPlayer":
                    return uuids(shared.get("connected_players")).contains((UUID) args[0]);
                case "getDisplayedItem": {
                    Map<String, Object> item = Nbt.map(shared, "display_item");
                    return item == null ? new ItemStack(org.bukkit.Material.AIR) : Convert.stack(item);
                }
                case "setDisplayedItem": {
                    ItemStack item = (ItemStack) args[0];
                    if (item == null || item.getType().isAir()) {
                        shared.remove("display_item");
                    } else {
                        shared.put("display_item", Convert.stack(item));
                    }
                    return null;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class HiveKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return Beehive.class;
        }

        private static int bees(Tile tile) {
            List<Object> list = Nbt.list(tile.data, "bees");
            return list == null ? 0 : list.size();
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getEntityCount":
                    return bees(tile);
                case "getMaxEntities":
                    return Nbt.integer(tile.data, "MaxBees", 3);
                case "setMaxEntities": {
                    int limit = (Integer) args[0];
                    if (limit < 1) {
                        throw new IllegalArgumentException("Max bees must be more than 0");
                    }
                    tile.put("MaxBees", limit);
                    return null;
                }
                case "isFull":
                    return bees(tile) >= Nbt.integer(tile.data, "MaxBees", 3);
                case "addEntity": {
                    org.bukkit.entity.Bee bee = (org.bukkit.entity.Bee) args[0];
                    if (bees(tile) >= Nbt.integer(tile.data, "MaxBees", 3)) {
                        throw new IllegalStateException("Hive is full");
                    }
                    org.bukkit.entity.EntitySnapshot snapshot = bee.createSnapshot();
                    if (!(snapshot instanceof org.basaltmc.bukkit.entity.BasaltEntitySnapshot own)) {
                        throw new IllegalArgumentException("The bee is not a valid entity");
                    }
                    Map<String, Object> occupant = Nbt.compound();
                    Map<String, Object> saved = Nbt.readNamed(own.nbt());
                    saved.remove("UUID");
                    occupant.put("entity_data", saved);
                    occupant.put("ticks_in_hive", 0);
                    occupant.put("min_ticks_in_hive", bee.hasNectar() ? 2400 : 600);
                    List<Object> list = Nbt.list(tile.data, "bees");
                    list = list == null ? new ArrayList<>() : new ArrayList<>(list);
                    list.add(occupant);
                    tile.put("bees", list);
                    bee.remove();
                    return null;
                }
                case "releaseEntities": {
                    List<Object> list = Nbt.list(tile.data, "bees");
                    List<org.bukkit.entity.Entity> released = new ArrayList<>();
                    if (list != null) {
                        Location above = tile.location().add(0.5, 1.0, 0.5);
                        for (Object entry : list) {
                            Map<String, Object> saved = Nbt.map(Nbt.map(entry), "entity_data");
                            org.bukkit.entity.EntitySnapshot snapshot = saved == null ? null
                                : org.basaltmc.bukkit.entity.BasaltEntitySnapshot.ofNbt(Nbt.writeNamed(saved));
                            org.bukkit.entity.Entity made = snapshot == null ? null : snapshot.createEntity(above);
                            if (made != null) {
                                released.add(made);
                            }
                        }
                    }
                    tile.put("bees", new ArrayList<Object>());
                    return released;
                }
                case "clearEntities":
                    tile.put("bees", new ArrayList<Object>());
                    return null;
                case "getFlower": {
                    Object raw = tile.data.get("flower_pos");
                    return raw instanceof int[] at && at.length >= 3 ? new Location(tile.world, at[0], at[1], at[2])
                        : null;
                }
                case "setFlower": {
                    Location at = (Location) args[0];
                    tile.put("flower_pos", at == null ? null : new int[] {at.getBlockX(), at.getBlockY(), at.getBlockZ()});
                    return null;
                }
                case "isSedated": {
                    org.bukkit.block.Block below = tile.live().getRelative(org.bukkit.block.BlockFace.DOWN);
                    return below.getType().name().endsWith("CAMPFIRE")
                        && below.getBlockData() instanceof org.bukkit.block.data.Lightable lit && lit.isLit();
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class BrushableKind implements TileKind {
        private final Class<? extends BlockState> face;

        BrushableKind(Class<? extends BlockState> face) {
            this.face = face;
        }

        @Override
        public Class<? extends BlockState> face() {
            return face;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getItem": {
                    Map<String, Object> item = tile.map("item");
                    return item == null ? new ItemStack(org.bukkit.Material.AIR) : Convert.stack(item);
                }
                case "setItem": {
                    ItemStack item = (ItemStack) args[0];
                    tile.put("item", item == null || item.getType().isAir() ? null : Convert.stack(item));
                    return null;
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class CreakingKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return CreakingHeart.class;
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getCreaking": {
                    UUID id = Convert.uuidOf(tile.data.get("creaking"));
                    Entity entity = id == null ? null : Bukkit.getEntity(id);
                    return entity instanceof org.bukkit.entity.Creaking creaking ? creaking : null;
                }
                case "setCreaking":
                    tile.put("creaking", args[0] == null ? null
                        : Convert.uuidToInts(((org.bukkit.entity.Creaking) args[0]).getUniqueId()));
                    return null;
                case "spawnCreaking": {
                    String id = WorldEnvNatives.creakingHeart(tile.world.getName(), "spawn", tile.x, tile.y, tile.z);
                    Entity entity = id == null ? null : Bukkit.getEntity(UUID.fromString(id));
                    if (entity instanceof org.bukkit.entity.Creaking creaking) {
                        tile.put("creaking", Convert.uuidToInts(creaking.getUniqueId()));
                        return creaking;
                    }
                    return null;
                }
                case "spreadResin": {
                    String at = WorldEnvNatives.creakingHeart(tile.world.getName(), "resin", tile.x, tile.y, tile.z);
                    if (at == null) {
                        return null;
                    }
                    String[] parts = at.split(",");
                    return new Location(tile.world, Integer.parseInt(parts[0]), Integer.parseInt(parts[1]),
                        Integer.parseInt(parts[2]));
                }
                default:
                    return Tile.kNone;
            }
        }
    }

    static final class TrialKind implements TileKind {
        @Override
        public Class<? extends BlockState> face() {
            return TrialSpawner.class;
        }

        private static LootTable table(Object id) {
            NamespacedKey key = id instanceof String text ? NamespacedKey.fromString(text) : null;
            return key == null ? null : Bukkit.getLootTable(key);
        }

        private static void checkReward(LootTable table, int weight) {
            if (table == null) {
                throw new IllegalArgumentException("Table cannot be null");
            }
            if (weight < 1) {
                throw new IllegalArgumentException("Weight must be at least 1");
            }
        }

        private static Map<String, Object> reward(LootTable table, int weight) {
            Map<String, Object> entry = Nbt.compound();
            entry.put("data", table.getKey().toString());
            entry.put("weight", weight);
            return entry;
        }

        private static List<Object> rewards(Map<String, Object> config) {
            List<Object> list = Nbt.list(config, "loot_tables_to_eject");
            if (list == null) {
                list = new ArrayList<>(List.of(reward(Bukkit.getLootTable(NamespacedKey.minecraft(
                    "spawners/trial_chamber/consumables")), 1), reward(Bukkit.getLootTable(NamespacedKey.minecraft(
                    "spawners/trial_chamber/key")), 1)));
            }
            return list;
        }

        private static Object potentials(Map<String, Object> config, String name, Object[] args) {
            Map<String, Object> scratch = Nbt.compound();
            List<Object> held = Nbt.list(config, "spawn_potentials");
            scratch.put("SpawnPotentials", held == null ? new ArrayList<Object>() : held);
            if (held != null && !held.isEmpty() && Nbt.map(held.get(0)) != null) {
                Map<String, Object> first = Nbt.map(Nbt.map(held.get(0)).get("data"));
                Map<String, Object> spawn = Nbt.compound();
                if (first != null && Nbt.map(first.get("entity")) != null) {
                    spawn.put("entity", Nbt.map(first.get("entity")));
                    scratch.put("SpawnData", spawn);
                }
            }
            Object result = SpawnerKind.callOn(new SpawnerView(null, scratch, new Location(null, 0, 0, 0)), name, args);
            if (!name.startsWith("get")) {
                config.put("spawn_potentials", scratch.get("SpawnPotentials"));
            }
            return result;
        }

        private static TrialSpawnerConfiguration configuration(Tile tile, String key) {
            return (TrialSpawnerConfiguration) Proxy.newProxyInstance(TrialSpawnerConfiguration.class.getClassLoader(),
                new Class<?>[] {TrialSpawnerConfiguration.class}, (proxy, method, args) -> {
                    String name = method.getName();
                    Map<String, Object> config = Nbt.map(tile.data.get(key)) != null ? Nbt.map(tile.data.get(key))
                        : name.startsWith("get") ? Nbt.compound() : tile.mapOrCreate(key);
                    switch (name) {
                        case "getBaseSpawnsBeforeCooldown":
                            return (float) Nbt.real(config, "total_mobs", 6.0);
                        case "setBaseSpawnsBeforeCooldown":
                            config.put("total_mobs", (Float) args[0]);
                            return null;
                        case "getBaseSimultaneousEntities":
                            return (float) Nbt.real(config, "simultaneous_mobs", 2.0);
                        case "setBaseSimultaneousEntities":
                            config.put("simultaneous_mobs", (Float) args[0]);
                            return null;
                        case "getAdditionalSpawnsBeforeCooldown":
                            return (float) Nbt.real(config, "total_mobs_added_per_player", 2.0);
                        case "setAdditionalSpawnsBeforeCooldown":
                            config.put("total_mobs_added_per_player", (Float) args[0]);
                            return null;
                        case "getAdditionalSimultaneousEntities":
                            return (float) Nbt.real(config, "simultaneous_mobs_added_per_player", 1.0);
                        case "setAdditionalSimultaneousEntities":
                            config.put("simultaneous_mobs_added_per_player", (Float) args[0]);
                            return null;
                        case "getSpawnRange":
                            return Nbt.integer(config, "spawn_range", 4);
                        case "setSpawnRange":
                            config.put("spawn_range", (Integer) args[0]);
                            return null;
                        case "getDelay":
                            return Nbt.integer(config, "ticks_between_spawn", 40);
                        case "setDelay":
                            if ((Integer) args[0] < 0) {
                                throw new IllegalArgumentException("Delay cannot be less than 0");
                            }
                            config.put("ticks_between_spawn", (Integer) args[0]);
                            return null;
                        case "getRequiredPlayerRange":
                            return tile.integer("required_player_range", 14);
                        case "setRequiredPlayerRange":
                            tile.put("required_player_range", (Integer) args[0]);
                            return null;
                        case "getSpawnedType":
                        case "getSpawnedEntity":
                        case "setSpawnedType":
                        case "setSpawnedEntity":
                        case "addPotentialSpawn":
                        case "setPotentialSpawns":
                        case "getPotentialSpawns":
                            return potentials(config, name, args);
                        case "getPossibleRewards": {
                            Map<LootTable, Integer> out = new java.util.LinkedHashMap<>();
                            for (Object value : rewards(config)) {
                                Map<String, Object> entry = Nbt.map(value);
                                LootTable table = entry == null ? null : table(entry.get("data"));
                                if (table != null) {
                                    out.put(table, Nbt.integer(entry, "weight", 1));
                                }
                            }
                            return out;
                        }
                        case "addPossibleReward": {
                            checkReward((LootTable) args[0], (Integer) args[1]);
                            List<Object> list = rewards(config);
                            list.add(reward((LootTable) args[0], (Integer) args[1]));
                            config.put("loot_tables_to_eject", list);
                            return null;
                        }
                        case "removePossibleReward": {
                            String rewardKey = ((LootTable) args[0]).getKey().toString();
                            List<Object> list = rewards(config);
                            list.removeIf(value -> rewardKey.equals(Nbt.map(value).get("data")));
                            config.put("loot_tables_to_eject", list);
                            return null;
                        }
                        case "setPossibleRewards": {
                            List<Object> list = new ArrayList<>();
                            Map<LootTable, Integer> given = (Map<LootTable, Integer>) args[0];
                            for (Map.Entry<LootTable, Integer> entry : given == null
                                ? Map.<LootTable, Integer>of().entrySet() : given.entrySet()) {
                                checkReward(entry.getKey(), entry.getValue());
                                list.add(reward(entry.getKey(), entry.getValue()));
                            }
                            config.put("loot_tables_to_eject", list);
                            return null;
                        }
                        case "hashCode":
                            return System.identityHashCode(proxy);
                        case "equals":
                            return proxy == args[0];
                        case "toString":
                            return "TrialSpawnerConfiguration" + key;
                        default:
                            throw Unsupported.hit("TrialSpawnerConfiguration#" + name);
                    }
                });
        }

        @Override
        public Object call(Tile tile, Method method, String name, Object[] args) {
            switch (name) {
                case "getCooldownEnd":
                    return Nbt.longValue(tile.data, "cooldown_ends_at", 0L);
                case "setCooldownEnd":
                    tile.put("cooldown_ends_at", (Long) args[0]);
                    return null;
                case "getNextSpawnAttempt":
                    return Nbt.longValue(tile.data, "next_mob_spawns_at", 0L);
                case "setNextSpawnAttempt":
                    tile.put("next_mob_spawns_at", (Long) args[0]);
                    return null;
                case "getCooldownLength":
                    return tile.integer("target_cooldown_length", 36000);
                case "setCooldownLength":
                    tile.put("target_cooldown_length", (Integer) args[0]);
                    return null;
                case "getRequiredPlayerRange":
                    return tile.integer("required_player_range", 14);
                case "setRequiredPlayerRange":
                    tile.put("required_player_range", (Integer) args[0]);
                    return null;
                case "isOminous":
                    return "true".equals(tile.property("ominous"));
                case "setOminous": {
                    String text = tile.blockData().getAsString();
                    String flipped = text.contains("ominous=") ? text.replaceAll("ominous=(true|false)", "ominous=" + args[0])
                        : text;
                    tile.base.setBlockData(Bukkit.createBlockData(flipped));
                    return null;
                }
                case "getTrackedPlayers": {
                    Collection<Player> out = new ArrayList<>();
                    for (UUID id : uuids(tile.data.get("registered_players"))) {
                        Player player = Bukkit.getPlayer(id);
                        if (player != null) {
                            out.add(player);
                        }
                    }
                    return out;
                }
                case "getTrackedEntities": {
                    Collection<Entity> out = new ArrayList<>();
                    for (UUID id : uuids(tile.data.get("current_mobs"))) {
                        Entity entity = Bukkit.getEntity(id);
                        if (entity != null) {
                            out.add(entity);
                        }
                    }
                    return out;
                }
                case "isTrackingPlayer":
                    return uuids(tile.data.get("registered_players")).contains(((Player) args[0]).getUniqueId());
                case "isTrackingEntity":
                    return uuids(tile.data.get("current_mobs")).contains(((Entity) args[0]).getUniqueId());
                case "startTrackingPlayer", "stopTrackingPlayer", "startTrackingEntity", "stopTrackingEntity": {
                    boolean player = name.endsWith("Player");
                    String key = player ? "registered_players" : "current_mobs";
                    Set<UUID> ids = uuids(tile.data.get(key));
                    UUID id = player ? ((Player) args[0]).getUniqueId() : ((Entity) args[0]).getUniqueId();
                    if (name.startsWith("start")) {
                        ids.add(id);
                    } else {
                        ids.remove(id);
                    }
                    tile.put(key, ints(ids));
                    return null;
                }
                case "getNormalConfiguration":
                    return configuration(tile, "normal_config");
                case "getOminousConfiguration":
                    return configuration(tile, "ominous_config");
                default:
                    return Tile.kNone;
            }
        }
    }
}
