package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Fox;

public interface FoxOps extends Fox, Cored {
    default Fox.Type getFoxType() {
        return "snow".equals(core().nbtText("Type")) ? Fox.Type.SNOW : Fox.Type.RED;
    }

    default void setFoxType(Fox.Type type) {
        core().nbtSet("Type", "string", type == Fox.Type.SNOW ? "snow" : "red");
    }

    private java.util.List<java.util.UUID> trusted() {
        java.util.List<java.util.UUID> out = new java.util.ArrayList<>();
        java.util.List<Object> stored = org.basaltmc.bukkit.item.Nbt.list(EntityNbt.compound(core(), "").get("Trusted"));
        for (Object id : stored == null ? java.util.List.of() : stored) {
            out.add(org.basaltmc.bukkit.item.Convert.uuidOf(id));
        }
        return out;
    }

    private void trust(int index, org.bukkit.entity.AnimalTamer player) {
        java.util.List<java.util.UUID> held = new java.util.ArrayList<>(trusted());
        while (held.size() <= index) {
            held.add(null);
        }
        held.set(index, player == null ? null : player.getUniqueId());
        held.removeIf(java.util.Objects::isNull);
        java.util.List<Object> stored = new java.util.ArrayList<>();
        for (java.util.UUID id : held) {
            stored.add(org.basaltmc.bukkit.item.Convert.uuidToInts(id));
        }
        if (stored.isEmpty()) {
            core().nbtRemove("Trusted");
        } else {
            EntityNbt.put(core(), "Trusted", stored);
        }
    }

    default org.bukkit.entity.AnimalTamer getFirstTrustedPlayer() {
        java.util.List<java.util.UUID> held = trusted();
        return held.isEmpty() ? null : org.bukkit.Bukkit.getOfflinePlayer(held.get(0));
    }

    default void setFirstTrustedPlayer(org.bukkit.entity.AnimalTamer player) {
        if (player == null && getSecondTrustedPlayer() != null) {
            throw new IllegalStateException("Must remove second trusted player first");
        }
        trust(0, player);
    }

    default org.bukkit.entity.AnimalTamer getSecondTrustedPlayer() {
        java.util.List<java.util.UUID> held = trusted();
        return held.size() < 2 ? null : org.bukkit.Bukkit.getOfflinePlayer(held.get(1));
    }

    default void setSecondTrustedPlayer(org.bukkit.entity.AnimalTamer player) {
        if (player != null && getFirstTrustedPlayer() == null) {
            throw new IllegalStateException("Must add first trusted player first");
        }
        trust(1, player);
    }

    default void setSleeping(boolean sleeping) {
        core().call("set_sleeping", sleeping);
    }

    private boolean flag(int bit) {
        return (core().integer("fox_flags") & bit) != 0;
    }

    private void flag(int bit, boolean on) {
        int flags = core().integer("fox_flags");
        core().set("fox_flags", on ? flags | bit : flags & ~bit);
    }

    default boolean isCrouching() {
        return flag(4);
    }

    default void setCrouching(boolean crouching) {
        flag(4, crouching);
    }

    default boolean isInterested() {
        return flag(8);
    }

    default void setInterested(boolean interested) {
        flag(8, interested);
    }

    default boolean isLeaping() {
        return flag(16);
    }

    default void setLeaping(boolean leaping) {
        flag(16, leaping);
    }

    default boolean isFaceplanted() {
        return flag(64);
    }

    default void setFaceplanted(boolean faceplanted) {
        flag(64, faceplanted);
    }

    default boolean isDefending() {
        return flag(128);
    }

    default void setDefending(boolean defending) {
        flag(128, defending);
    }
}
