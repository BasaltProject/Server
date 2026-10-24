package org.basaltmc.bukkit.entity.ops;

import com.destroystokyo.paper.loottable.LootableEntityInventory;
import java.util.UUID;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Entity;

/** Loot bookkeeping of a container entity; the server fills it on the first opening and never refills it. */
public interface LootableEntityInventoryOps extends LootableEntityInventory, Cored {
    private java.util.Map<String, Object> data() {
        java.util.Map<String, Object> found = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "")
            .get("Paper.LootableData"));
        return found == null ? org.basaltmc.bukkit.item.Nbt.compound() : found;
    }

    private void save(java.util.Map<String, Object> data) {
        java.util.Map<String, Object> patch = org.basaltmc.bukkit.item.Nbt.compound();
        patch.put("Paper.LootableData", data);
        core().mergeNbt(org.basaltmc.bukkit.item.Nbt.writeNamed(patch));
    }

    private java.util.Map<String, Object> looted() {
        java.util.Map<String, Object> found = org.basaltmc.bukkit.item.Nbt.map(data().get("lootedPlayers"));
        return found == null ? org.basaltmc.bukkit.item.Nbt.compound() : found;
    }

    default Entity getEntity() {
        return (Entity) this;
    }

    default void setLootTable(org.bukkit.loot.LootTable table, long seed) {
        setLootTable(table);
        setSeed(seed);
    }

    default boolean isRefillEnabled() {
        return false;
    }

    default boolean hasBeenFilled() {
        return getLastFilled() != -1L || !hasLootTable();
    }

    default boolean canPlayerLoot(UUID player) {
        return true;
    }

    default boolean hasPlayerLooted(UUID player) {
        return looted().containsKey(player.toString());
    }

    default Long getLastLooted(UUID player) {
        return looted().get(player.toString()) instanceof Number time ? time.longValue() : null;
    }

    default boolean setHasPlayerLooted(UUID player, boolean looted) {
        boolean was = hasPlayerLooted(player);
        java.util.Map<String, Object> all = looted();
        if (looted) {
            all.put(player.toString(), System.currentTimeMillis());
        } else {
            all.remove(player.toString());
        }
        java.util.Map<String, Object> data = data();
        data.put("lootedPlayers", all);
        save(data);
        return was;
    }

    default boolean hasPendingRefill() {
        long next = getNextRefill();
        return next != -1L && next < System.currentTimeMillis();
    }

    default long getLastFilled() {
        return data().get("lastFill") instanceof Number time ? time.longValue() : -1L;
    }

    default long getNextRefill() {
        return data().get("nextRefill") instanceof Number time ? time.longValue() : -1L;
    }

    default long setNextRefill(long refillAt) {
        long previous = getNextRefill();
        java.util.Map<String, Object> data = data();
        data.put("nextRefill", refillAt);
        save(data);
        return previous;
    }
}
