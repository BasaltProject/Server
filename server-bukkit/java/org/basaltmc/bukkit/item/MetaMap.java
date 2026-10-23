package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.Bukkit;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.MapMeta;
import org.bukkit.map.MapView;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaMap extends MetaItem implements MapMeta {
    public MetaMap(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaMap clone() {
        return (MetaMap) super.clone();
    }

    @Override
    public boolean hasMapId() {
        return c.containsKey("minecraft:map_id");
    }

    @Override
    public int getMapId() {
        if (!hasMapId()) {
            throw new IllegalStateException("Item does not have map associated - check hasMapId() first!");
        }
        return Nbt.integer(c, "minecraft:map_id", 0);
    }

    @Override
    public void setMapId(int id) {
        c.put("minecraft:map_id", id);
    }

    @Override
    public boolean hasMapView() {
        return hasMapId();
    }

    @Override
    public MapView getMapView() {
        return hasMapId() ? Bukkit.getMap(getMapId()) : null;
    }

    @Override
    public void setMapView(MapView map) {
        if (map == null) {
            c.remove("minecraft:map_id");
        } else {
            setMapId(map.getId());
        }
    }

    @Override
    public boolean isScaling() {
        return "scale".equals(c.get("minecraft:map_post_processing"));
    }

    @Override
    public void setScaling(boolean scaling) {
        if (scaling) {
            c.put("minecraft:map_post_processing", "scale");
        } else if (isScaling()) {
            c.remove("minecraft:map_post_processing");
        }
    }
}
