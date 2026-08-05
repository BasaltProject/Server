package org.basaltmc.bukkit.tile;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.basaltmc.bukkit.RestNatives;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;

public final class LootableData {
    private static final String kRoot = "Paper.LootableData";

    private final Map<String, Object> root;

    private LootableData(Map<String, Object> root) {
        this.root = root;
    }

    public static LootableData of(Map<String, Object> container) {
        Map<String, Object> found = Nbt.map(container.get(kRoot));
        return new LootableData(found == null ? Nbt.compound() : found);
    }

    /** The compound to store back under Paper.LootableData, or null when nothing is left in it. */
    public Map<String, Object> toNbt() {
        return root.isEmpty() ? null : root;
    }

    public static String key() {
        return kRoot;
    }

    public long lastFill() {
        return Nbt.longValue(root, "lastFill", -1L);
    }

    public long nextRefill() {
        return Nbt.longValue(root, "nextRefill", -1L);
    }

    public long setNextRefill(long refillAt) {
        long previous = nextRefill();
        root.put("nextRefill", refillAt);
        return previous;
    }

    public boolean hasPendingRefill() {
        long next = nextRefill();
        return next != -1L && next < System.currentTimeMillis();
    }

    private List<Object> looted() {
        List<Object> list = Nbt.list(root, "lootedPlayers");
        return list == null ? new ArrayList<>() : list;
    }

    private Map<String, Object> entry(UUID player) {
        for (Object value : looted()) {
            Map<String, Object> entry = Nbt.map(value);
            if (entry != null && player.equals(Convert.uuidOf(entry.get("UUID")))) {
                return entry;
            }
        }
        return null;
    }

    public boolean hasPlayerLooted(UUID player) {
        return entry(player) != null;
    }

    public Long lastLooted(UUID player) {
        Map<String, Object> entry = entry(player);
        return entry == null ? null : Nbt.longValue(entry, "Time", 0L);
    }

    public boolean setHasPlayerLooted(UUID player, boolean looted) {
        boolean was = hasPlayerLooted(player);
        if (was == looted) {
            return was;
        }
        List<Object> list = looted();
        if (looted) {
            Map<String, Object> entry = Nbt.compound();
            entry.put("UUID", Convert.uuidToInts(player));
            entry.put("Time", System.currentTimeMillis());
            list.add(entry);
        } else {
            list.removeIf(value -> player.equals(Convert.uuidOf(Nbt.map(value).get("UUID"))));
        }
        if (list.isEmpty()) {
            root.remove("lootedPlayers");
        } else {
            root.put("lootedPlayers", list);
        }
        return was;
    }

    public boolean canPlayerLoot(String world, UUID player) {
        Long time = lastLooted(player);
        if (time == null) {
            return true;
        }
        double wait = RestNatives.worldTunable(world, "loot_reloot_seconds");
        return wait >= 0 && time + (long) wait * 1000L < System.currentTimeMillis();
    }

    public static boolean refillEnabled(String world) {
        return RestNatives.worldTunable(world, "loot_refill") == 1.0;
    }
}
