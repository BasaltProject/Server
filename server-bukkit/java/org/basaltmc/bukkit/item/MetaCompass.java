package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.World;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.CompassMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaCompass extends MetaItem implements CompassMeta {
    private static final String kTracker = "minecraft:lodestone_tracker";

    public MetaCompass(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaCompass clone() {
        return (MetaCompass) super.clone();
    }

    private Map<String, Object> target() {
        Map<String, Object> tracker = compoundOf(kTracker);
        return tracker == null ? null : Nbt.map(tracker.get("target"));
    }

    @Override
    public boolean hasLodestone() {
        return target() != null;
    }

    @Override
    public Location getLodestone() {
        Map<String, Object> target = target();
        if (target == null) {
            return null;
        }
        NamespacedKey dimension = NamespacedKey.fromString(Nbt.string(target, "dimension", "minecraft:overworld"));
        World world = dimension == null ? null : Bukkit.getWorld(dimension);
        if (world == null) {
            return null;
        }
        Object pos = target.get("pos");
        if (pos instanceof int[] ints && ints.length == 3) {
            return new Location(world, ints[0], ints[1], ints[2]);
        }
        return null;
    }

    @Override
    public void setLodestone(Location location) {
        boolean tracked = isLodestoneTracked();
        if (location == null || location.getWorld() == null) {
            clearLodestone();
            return;
        }
        Map<String, Object> target = Nbt.compound();
        target.put("pos", new int[] {location.getBlockX(), location.getBlockY(), location.getBlockZ()});
        target.put("dimension", location.getWorld().getKey().toString());
        Map<String, Object> tracker = Nbt.compound();
        tracker.put("target", target);
        tracker.put("tracked", Nbt.bool(tracked));
        c.put(kTracker, tracker);
    }

    @Override
    public boolean isLodestoneTracked() {
        return Nbt.flag(compoundOf(kTracker), "tracked", true);
    }

    @Override
    public void setLodestoneTracked(boolean tracked) {
        Map<String, Object> tracker = compoundOf(kTracker);
        if (tracker == null) {
            tracker = Nbt.compound();
            c.put(kTracker, tracker);
        }
        tracker.put("tracked", Nbt.bool(tracked));
    }

    @Override
    public boolean isLodestoneCompass() {
        return c.containsKey(kTracker);
    }

    @Override
    public void clearLodestone() {
        c.remove(kTracker);
    }
}
