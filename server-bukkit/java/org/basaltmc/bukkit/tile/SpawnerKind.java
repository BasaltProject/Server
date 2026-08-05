package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.entity.BasaltEntitySnapshot;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.NamespacedKey;
import org.bukkit.block.BlockState;
import org.bukkit.block.CreatureSpawner;
import org.bukkit.block.spawner.SpawnRule;
import org.bukkit.block.spawner.SpawnerEntry;
import org.bukkit.entity.EntitySnapshot;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;

public final class SpawnerKind implements TileKind {
    @Override
    public Class<? extends BlockState> face() {
        return CreatureSpawner.class;
    }

    private static void number(SpawnerView tile, String key, Object value) {
        tile.put(key, ((Number) value).shortValue());
    }

    private static Map<String, Object> entity(SpawnerView tile) {
        Map<String, Object> spawn = tile.map("SpawnData");
        return spawn == null ? null : Nbt.map(spawn, "entity");
    }

    static EntitySnapshot snapshot(Map<String, Object> entity) {
        return entity == null || !(entity.get("id") instanceof String) ? null
            : BasaltEntitySnapshot.ofNbt(Nbt.writeNamed(entity));
    }

    static Map<String, Object> compound(EntitySnapshot snapshot) {
        return Nbt.readNamed(((BasaltEntitySnapshot) snapshot).nbt());
    }

    private static Map<String, Object> light(int min, int max) {
        Map<String, Object> limit = Nbt.compound();
        limit.put("min_inclusive", min);
        limit.put("max_inclusive", max);
        return limit;
    }

    static Map<String, Object> potential(EntitySnapshot snapshot, int weight, SpawnRule rule) {
        Map<String, Object> data = Nbt.compound();
        data.put("entity", compound(snapshot));
        if (rule != null) {
            Map<String, Object> rules = Nbt.compound();
            rules.put("block_light_limit", light(rule.getMinBlockLight(), rule.getMaxBlockLight()));
            rules.put("sky_light_limit", light(rule.getMinSkyLight(), rule.getMaxSkyLight()));
            data.put("custom_spawn_rules", rules);
        }
        Map<String, Object> entry = Nbt.compound();
        entry.put("weight", weight);
        entry.put("data", data);
        return entry;
    }

    static SpawnerEntry entryOf(Object value) {
        Map<String, Object> entry = Nbt.map(value);
        Map<String, Object> data = entry == null ? null : Nbt.map(entry, "data");
        EntitySnapshot snapshot = data == null ? null : snapshot(Nbt.map(data, "entity"));
        if (snapshot == null) {
            return null;
        }
        Map<String, Object> rules = Nbt.map(data, "custom_spawn_rules");
        SpawnRule rule = null;
        if (rules != null) {
            Map<String, Object> block = Nbt.map(rules, "block_light_limit");
            Map<String, Object> sky = Nbt.map(rules, "sky_light_limit");
            rule = new SpawnRule(block == null ? 0 : Nbt.integer(block, "min_inclusive", 0),
                block == null ? 15 : Nbt.integer(block, "max_inclusive", 15),
                sky == null ? 0 : Nbt.integer(sky, "min_inclusive", 0),
                sky == null ? 15 : Nbt.integer(sky, "max_inclusive", 15));
        }
        return new SpawnerEntry(snapshot, Nbt.integer(entry, "weight", 1), rule);
    }

    private static void setSingle(SpawnerView tile, Map<String, Object> entity) {
        Map<String, Object> spawn = Nbt.compound();
        spawn.put("entity", entity);
        tile.put("SpawnData", spawn);
        Map<String, Object> data = Nbt.compound();
        data.put("entity", Nbt.deepCopy(entity));
        Map<String, Object> entry = Nbt.compound();
        entry.put("weight", 1);
        entry.put("data", data);
        List<Object> potentials = new ArrayList<>();
        potentials.add(entry);
        tile.put("SpawnPotentials", potentials);
    }

    @Override
    public Object call(Tile tile, Method method, String name, Object[] args) {
        return callOn(new SpawnerView(tile.world, tile.data, tile.location()), name, args);
    }

    /** Runs a spawner call on plain data, for the block and for the minecart that spawns just the same. */
    public static Object callOn(SpawnerView tile, String name, Object[] args) {
        int count = args == null ? 0 : args.length;
        switch (name) {
            case "getSpawnedType": {
                Map<String, Object> entity = entity(tile);
                String id = entity == null ? null : Nbt.string(entity, "id", null);
                NamespacedKey key = id == null ? null : NamespacedKey.fromString(id);
                return key == null ? null : org.bukkit.Registry.ENTITY_TYPE.get(key);
            }
            case "setSpawnedType": {
                EntityType type = (EntityType) args[0];
                if (type == null) {
                    tile.put("SpawnData", null);
                    tile.put("SpawnPotentials", new ArrayList<Object>());
                    return null;
                }
                if (type == EntityType.UNKNOWN) {
                    throw new IllegalArgumentException("Can't spawn EntityType " + type + " from mobspawners!");
                }
                Map<String, Object> entity = Nbt.compound();
                entity.put("id", type.getKey().toString());
                setSingle(tile, entity);
                return null;
            }
            case "getCreatureTypeName": {
                Map<String, Object> entity = entity(tile);
                String id = entity == null ? null : Nbt.string(entity, "id", null);
                return id == null ? "" : id.substring(id.indexOf(':') + 1);
            }
            case "setCreatureTypeByName": {
                EntityType type = EntityType.fromName((String) args[0]);
                if (type == null || type == EntityType.UNKNOWN) {
                    return null;
                }
                Map<String, Object> entity = Nbt.compound();
                entity.put("id", type.getKey().toString());
                setSingle(tile, entity);
                return null;
            }
            case "getDelay":
                return tile.integer("Delay", 20);
            case "setDelay":
                number(tile, "Delay", args[0]);
                return null;
            case "getMinSpawnDelay":
                return tile.integer("MinSpawnDelay", 200);
            case "setMinSpawnDelay":
                number(tile, "MinSpawnDelay", args[0]);
                return null;
            case "getMaxSpawnDelay":
                return tile.integer("MaxSpawnDelay", 800);
            case "setMaxSpawnDelay":
                number(tile, "MaxSpawnDelay", args[0]);
                return null;
            case "getSpawnCount":
                return tile.integer("SpawnCount", 4);
            case "setSpawnCount":
                number(tile, "SpawnCount", args[0]);
                return null;
            case "getMaxNearbyEntities":
                return tile.integer("MaxNearbyEntities", 6);
            case "setMaxNearbyEntities":
                number(tile, "MaxNearbyEntities", args[0]);
                return null;
            case "getRequiredPlayerRange":
                return tile.integer("RequiredPlayerRange", 16);
            case "setRequiredPlayerRange":
                number(tile, "RequiredPlayerRange", args[0]);
                return null;
            case "getSpawnRange":
                return tile.integer("SpawnRange", 4);
            case "setSpawnRange":
                number(tile, "SpawnRange", args[0]);
                return null;
            case "isActivated": {
                double range = tile.integer("RequiredPlayerRange", 16);
                for (Player player : tile.world.getPlayers()) {
                    if (player.getLocation().distanceSquared(tile.location().add(0.5, 0.5, 0.5)) <= range * range) {
                        return true;
                    }
                }
                return false;
            }
            case "resetTimer": {
                int min = tile.integer("MinSpawnDelay", 200);
                int max = Math.max(min, tile.integer("MaxSpawnDelay", 800));
                tile.put("Delay", (short) (min + ThreadLocalRandom.current().nextInt(max - min + 1)));
                return null;
            }
            case "setSpawnedItem": {
                ItemStack item = (ItemStack) args[0];
                Map<String, Object> entity = Nbt.compound();
                entity.put("id", "minecraft:item");
                entity.put("Item", org.basaltmc.bukkit.item.Convert.stack(item));
                setSingle(tile, entity);
                return null;
            }
            case "getSpawnedEntity":
                return snapshot(entity(tile));
            case "setSpawnedEntity":
                if (args[0] instanceof SpawnerEntry entry) {
                    Map<String, Object> entity = compound(entry.getSnapshot());
                    setSingle(tile, entity);
                    Map<String, Object> first = Nbt.map(Nbt.list(tile.data, "SpawnPotentials").get(0));
                    first.put("weight", entry.getSpawnWeight());
                    return null;
                }
                setSingle(tile, compound((EntitySnapshot) args[0]));
                return null;
            case "addPotentialSpawn": {
                List<Object> potentials = Nbt.list(tile.data, "SpawnPotentials");
                if (potentials == null) {
                    potentials = new ArrayList<>();
                    tile.put("SpawnPotentials", potentials);
                }
                if (count == 1) {
                    SpawnerEntry entry = (SpawnerEntry) args[0];
                    potentials.add(potential(entry.getSnapshot(), entry.getSpawnWeight(), entry.getSpawnRule()));
                } else {
                    potentials.add(potential((EntitySnapshot) args[0], (Integer) args[1], (SpawnRule) args[2]));
                }
                return null;
            }
            case "setPotentialSpawns": {
                List<Object> potentials = new ArrayList<>();
                for (SpawnerEntry entry : (Collection<SpawnerEntry>) args[0]) {
                    potentials.add(potential(entry.getSnapshot(), entry.getSpawnWeight(), entry.getSpawnRule()));
                }
                tile.put("SpawnPotentials", potentials);
                return null;
            }
            case "getPotentialSpawns": {
                List<SpawnerEntry> out = new ArrayList<>();
                List<Object> potentials = Nbt.list(tile.data, "SpawnPotentials");
                for (Object value : potentials == null ? List.of() : potentials) {
                    SpawnerEntry entry = entryOf(value);
                    if (entry != null) {
                        out.add(entry);
                    }
                }
                return out;
            }
            default:
                return Tile.kNone;
        }
    }
}
