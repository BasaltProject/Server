package org.basaltmc.bukkit.entity.ops;

import com.destroystokyo.paper.loottable.LootableEntityInventory;
import java.util.UUID;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.tile.LootableData;
import org.bukkit.entity.Entity;

/** Loot bookkeeping of a container entity, kept in the data the server writes under Paper.LootableData. */
public interface LootableEntityInventoryOps extends LootableEntityInventory, Cored {
    private LootableData data() {
        return LootableData.of(EntityNbt.compound(core(), ""));
    }

    private void save(LootableData data) {
        java.util.Map<String, Object> patch = Nbt.compound();
        patch.put(LootableData.key(), data.toNbt() == null ? Nbt.compound() : data.toNbt());
        core().mergeNbt(Nbt.writeNamed(patch));
    }

    default Entity getEntity() {
        return (Entity) this;
    }

    default void setLootTable(org.bukkit.loot.LootTable table, long seed) {
        setLootTable(table);
        setSeed(seed);
    }

    default boolean isRefillEnabled() {
        return LootableData.refillEnabled(core().world.getName());
    }

    default boolean hasBeenFilled() {
        return getLastFilled() != -1L;
    }

    default boolean canPlayerLoot(UUID player) {
        return data().canPlayerLoot(core().world.getName(), player);
    }

    default boolean hasPlayerLooted(UUID player) {
        return data().hasPlayerLooted(player);
    }

    default Long getLastLooted(UUID player) {
        return data().lastLooted(player);
    }

    default boolean setHasPlayerLooted(UUID player, boolean looted) {
        LootableData data = data();
        boolean was = data.setHasPlayerLooted(player, looted);
        save(data);
        return was;
    }

    default boolean hasPendingRefill() {
        return data().hasPendingRefill();
    }

    default long getLastFilled() {
        return data().lastFill();
    }

    default long getNextRefill() {
        return data().nextRefill();
    }

    default long setNextRefill(long refillAt) {
        LootableData data = data();
        long previous = data.setNextRefill(refillAt);
        save(data);
        return previous;
    }
}
