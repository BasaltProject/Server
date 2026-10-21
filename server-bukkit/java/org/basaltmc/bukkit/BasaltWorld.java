package org.basaltmc.bukkit;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.block.Block;
import org.bukkit.entity.Player;
import org.basaltmc.bukkit.gen.WorldBase;

/** A Basalt world by name; the blocks and time are read from the server when asked. */
public final class BasaltWorld extends WorldBase {
    private final String name;
    private final UUID id;

    BasaltWorld(String name) {
        this.name = name;
        this.id = UUID.nameUUIDFromBytes(("basalt-world:" + name).getBytes(StandardCharsets.UTF_8));
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public UUID getUID() {
        return id;
    }

    @Override
    public long getTime() {
        return Natives.worldTime(name) % 24000L;
    }

    @Override
    public long getFullTime() {
        return Natives.worldTime(name);
    }

    @Override
    public Block getBlockAt(int x, int y, int z) {
        return new BasaltBlock(this, x, y, z);
    }

    @Override
    public Block getBlockAt(Location location) {
        return getBlockAt(location.getBlockX(), location.getBlockY(), location.getBlockZ());
    }

    @Override
    public Location getSpawnLocation() {
        return new Location(this, 0, 64, 0);
    }

    @Override
    public List<Player> getPlayers() {
        List<Player> inside = new ArrayList<>();
        for (Player player : BasaltServer.instance().getOnlinePlayers()) {
            if (player.getWorld().getName().equals(name)) {
                inside.add(player);
            }
        }
        return inside;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltWorld world && world.name.equals(name);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltWorld{name=" + name + "}";
    }

    Material materialAt(int x, int y, int z) {
        String id = Natives.blockAt(name, x, y, z);
        if (id == null) {
            return Material.AIR;
        }
        Material material = Material.matchMaterial(id);
        return material == null ? Material.AIR : material;
    }
}
