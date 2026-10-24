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
        core().nbtSet("Sleeping", "bool", sleeping);
    }
}
