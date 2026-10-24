package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.entity.EntitySnapshot;
import org.bukkit.entity.EntityType;
import org.bukkit.inventory.meta.SpawnEggMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaSpawnEgg extends MetaItem implements SpawnEggMeta {
    private static final String kData = "minecraft:entity_data";

    public MetaSpawnEgg(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaSpawnEgg clone() {
        return (MetaSpawnEgg) super.clone();
    }

    @Override
    public EntityType getSpawnedType() {
        String name = type.getKey().getKey();
        if (!name.endsWith("_spawn_egg")) {
            return null;
        }
        return Registry.ENTITY_TYPE.get(NamespacedKey.minecraft(name.substring(0, name.length() - 10)));
    }

    @Override
    public void setSpawnedType(EntityType entityType) {
        setCustomSpawnedType(entityType);
    }

    @Override
    public EntityType getCustomSpawnedType() {
        Map<String, Object> data = compoundOf(kData);
        NamespacedKey key = data == null ? null : NamespacedKey.fromString(Nbt.string(data, "id", ""));
        return key == null ? null : Registry.ENTITY_TYPE.get(key);
    }

    @Override
    public void setCustomSpawnedType(EntityType entityType) {
        if (entityType == null) {
            c.remove(kData);
            return;
        }
        Map<String, Object> data = Nbt.compound();
        data.put("id", entityType.getKey().toString());
        c.put(kData, data);
    }

    @Override
    public EntitySnapshot getSpawnedEntity() {
        Map<String, Object> data = compoundOf(kData);
        return data == null ? null : org.basaltmc.bukkit.entity.BasaltEntitySnapshot.ofNbt(Nbt.writeNamed(data));
    }

    @Override
    public void setSpawnedEntity(EntitySnapshot snapshot) {
        if (snapshot == null) {
            c.remove(kData);
            return;
        }
        if (!(snapshot instanceof org.basaltmc.bukkit.entity.BasaltEntitySnapshot own)) {
            throw new IllegalArgumentException("Only snapshots taken from this server can be stored in a spawn egg");
        }
        c.put(kData, Nbt.readNamed(own.nbt()));
    }
}
