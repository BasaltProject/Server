package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.entity.Villager;

public interface VillagerOps extends Villager, Cored {
    default Villager.Profession getProfession() {
        String name = core().text("profession");
        Villager.Profession found = name == null ? null : Registry.VILLAGER_PROFESSION.get(NamespacedKey.minecraft(name));
        return found == null ? Villager.Profession.NONE : found;
    }

    default void setProfession(Villager.Profession profession) {
        core().set("profession", profession.getKey().getKey());
    }

    default int getVillagerLevel() {
        return core().integer("villager_level");
    }

    default void setVillagerLevel(int level) {
        if (level < 1 || level > 5) {
            throw new IllegalArgumentException("Level must be between 1 and 5");
        }
        core().set("villager_level", level);
    }

    default int getVillagerExperience() {
        return core().integer("villager_experience");
    }

    default void setVillagerExperience(int experience) {
        core().set("villager_experience", Math.max(0, experience));
    }

    default Villager.Type getVillagerType() {
        String name = core().nbtText("VillagerData.type");
        Villager.Type found = name == null ? null : Registry.VILLAGER_TYPE.get(NamespacedKey.fromString(name));
        return found == null ? Villager.Type.PLAINS : found;
    }

    default void setVillagerType(Villager.Type type) {
        core().nbtSet("VillagerData.type", "string", type.getKey().toString());
    }

    default boolean increaseLevel(int amount) {
        if (amount < 1 || getVillagerLevel() + amount > 5) {
            throw new IllegalArgumentException("Level must stay between 1 and 5");
        }
        setVillagerLevel(getVillagerLevel() + amount);
        return true;
    }

    default boolean addTrades(int amount) {
        if (amount < 1) {
            throw new IllegalArgumentException("Amount must be positive");
        }
        return core().call("villager_add_trades", (long) amount) instanceof Number added && added.intValue() > 0;
    }

    default int getRestocksToday() {
        return core().integer("villager_restocks");
    }

    default void setRestocksToday(int restocksToday) {
        core().set("villager_restocks", restocksToday);
    }

    default boolean sleep(org.bukkit.Location location) {
        if (!core().worldRef().equals(location.getWorld())) {
            throw new IllegalArgumentException("Cannot sleep in another world");
        }
        return core().test("villager_sleep", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ());
    }

    default void wakeup() {
        core().call("villager_wake");
    }

    default void shakeHead() {
        core().call("villager_shake_head");
    }

    default org.bukkit.entity.ZombieVillager zombify() {
        if (!(core().call("villager_zombify") instanceof Number id) || id.intValue() < 0) {
            return null;
        }
        return core().entity(id.intValue()) instanceof org.bukkit.entity.ZombieVillager made ? made : null;
    }

    default void updateDemand() {
        core().call("villager_update_demand");
    }

    default void restock() {
        core().call("villager_restock");
    }

    default void resetOffers() {
        core().call("villager_reset_offers");
    }

    default com.destroystokyo.paper.entity.villager.Reputation getReputation(java.util.UUID uniqueId) {
        return getReputations().get(uniqueId);
    }

    default java.util.Map<java.util.UUID, com.destroystokyo.paper.entity.villager.Reputation> getReputations() {
        java.util.Map<java.util.UUID, com.destroystokyo.paper.entity.villager.Reputation> out = new java.util.HashMap<>();
        if (core().call("villager_gossips") instanceof String[] flat) {
            com.destroystokyo.paper.entity.villager.ReputationType[] types =
                com.destroystokyo.paper.entity.villager.ReputationType.values();
            for (int at = 0; at + 1 + types.length <= flat.length; at += 1 + types.length) {
                com.destroystokyo.paper.entity.villager.Reputation reputation =
                    new com.destroystokyo.paper.entity.villager.Reputation();
                for (int type = 0; type < types.length; type++) {
                    int value = Integer.parseInt(flat[at + 1 + type]);
                    if (value != 0) {
                        reputation.setReputation(types[type], value);
                    }
                }
                try {
                    out.put(java.util.UUID.fromString(flat[at]), reputation);
                } catch (IllegalArgumentException notAPlayer) {
                    continue;
                }
            }
        }
        return out;
    }

    default void setReputation(java.util.UUID uniqueId, com.destroystokyo.paper.entity.villager.Reputation reputation) {
        com.destroystokyo.paper.entity.villager.ReputationType[] types =
            com.destroystokyo.paper.entity.villager.ReputationType.values();
        for (int type = 0; type < types.length; type++) {
            core().call("villager_set_gossip", uniqueId.toString(), (long) type,
                (long) reputation.getReputation(types[type]));
        }
    }

    default void setReputations(java.util.Map<java.util.UUID, com.destroystokyo.paper.entity.villager.Reputation>
            reputations) {
        clearReputations();
        for (java.util.Map.Entry<java.util.UUID, com.destroystokyo.paper.entity.villager.Reputation> entry
                : reputations.entrySet()) {
            setReputation(entry.getKey(), entry.getValue());
        }
    }

    default void clearReputations() {
        core().call("villager_clear_gossips");
    }

    default org.bukkit.inventory.Inventory getInventory() {
        return new org.basaltmc.bukkit.inv.HeldInventory(core(), this, "villager", 8);
    }
}
