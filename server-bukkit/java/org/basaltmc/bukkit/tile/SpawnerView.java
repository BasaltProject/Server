package org.basaltmc.bukkit.tile;

import java.util.Map;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Location;
import org.bukkit.World;

public final class SpawnerView {
    public final World world;
    public final Map<String, Object> data;
    private final Location origin;

    public SpawnerView(World world, Map<String, Object> data, Location origin) {
        this.world = world;
        this.data = data;
        this.origin = origin;
    }

    public Location location() {
        return origin.clone();
    }

    public int integer(String key, int fallback) {
        return Nbt.integer(data, key, fallback);
    }

    public Map<String, Object> map(String key) {
        return Nbt.map(data, key);
    }

    public void put(String key, Object value) {
        if (value == null) {
            data.remove(key);
        } else {
            data.put(key, value);
        }
    }
}
