package org.basaltmc.bukkit.entity.ops;

import java.util.Collection;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.tile.SpawnerKind;
import org.basaltmc.bukkit.tile.SpawnerView;
import org.bukkit.block.spawner.SpawnRule;
import org.bukkit.block.spawner.SpawnerEntry;
import org.bukkit.entity.EntitySnapshot;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.minecart.SpawnerMinecart;
import org.bukkit.inventory.ItemStack;

public interface SpawnerMinecartOps extends SpawnerMinecart, Cored {
    private Object spawner(String name, boolean writes, Object... args) {
        byte[] stored = core().nbt("");
        Map<String, Object> data = stored == null ? Nbt.compound() : Nbt.readNamed(stored);
        Object result = SpawnerKind.callOn(new SpawnerView(core().worldRef(), data, getLocation()), name, args);
        if (writes) {
            core().mergeNbt(Nbt.writeNamed(data));
        }
        return result;
    }

    default EntityType getSpawnedType() {
        return (EntityType) spawner("getSpawnedType", false);
    }

    default void setSpawnedType(EntityType type) {
        spawner("setSpawnedType", true, type);
    }

    default int getDelay() {
        return (Integer) spawner("getDelay", false);
    }

    default void setDelay(int delay) {
        spawner("setDelay", true, delay);
    }

    default int getRequiredPlayerRange() {
        return (Integer) spawner("getRequiredPlayerRange", false);
    }

    default void setRequiredPlayerRange(int range) {
        spawner("setRequiredPlayerRange", true, range);
    }

    default int getSpawnRange() {
        return (Integer) spawner("getSpawnRange", false);
    }

    default void setSpawnRange(int range) {
        spawner("setSpawnRange", true, range);
    }

    default EntitySnapshot getSpawnedEntity() {
        return (EntitySnapshot) spawner("getSpawnedEntity", false);
    }

    default void setSpawnedEntity(EntitySnapshot snapshot) {
        spawner("setSpawnedEntity", true, snapshot);
    }

    default void setSpawnedEntity(SpawnerEntry entry) {
        spawner("setSpawnedEntity", true, entry);
    }

    default void addPotentialSpawn(EntitySnapshot snapshot, int weight, SpawnRule rule) {
        spawner("addPotentialSpawn", true, snapshot, weight, rule);
    }

    default void addPotentialSpawn(SpawnerEntry entry) {
        spawner("addPotentialSpawn", true, entry);
    }

    default void setPotentialSpawns(Collection<SpawnerEntry> entries) {
        spawner("setPotentialSpawns", true, entries);
    }

    @SuppressWarnings("unchecked")
    default List<SpawnerEntry> getPotentialSpawns() {
        return (List<SpawnerEntry>) spawner("getPotentialSpawns", false);
    }

    default int getMinSpawnDelay() {
        return (Integer) spawner("getMinSpawnDelay", false);
    }

    default void setMinSpawnDelay(int delay) {
        spawner("setMinSpawnDelay", true, delay);
    }

    default int getMaxSpawnDelay() {
        return (Integer) spawner("getMaxSpawnDelay", false);
    }

    default void setMaxSpawnDelay(int delay) {
        spawner("setMaxSpawnDelay", true, delay);
    }

    default int getSpawnCount() {
        return (Integer) spawner("getSpawnCount", false);
    }

    default void setSpawnCount(int count) {
        spawner("setSpawnCount", true, count);
    }

    default int getMaxNearbyEntities() {
        return (Integer) spawner("getMaxNearbyEntities", false);
    }

    default void setMaxNearbyEntities(int count) {
        spawner("setMaxNearbyEntities", true, count);
    }

    default boolean isActivated() {
        return (Boolean) spawner("isActivated", false);
    }

    default void resetTimer() {
        spawner("resetTimer", true);
    }

    default void setSpawnedItem(ItemStack item) {
        spawner("setSpawnedItem", true, item);
    }
}
