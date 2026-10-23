package org.basaltmc.bukkit.entity.ops;

import java.util.UUID;
import org.bukkit.entity.AnimalTamer;
import org.bukkit.entity.Tameable;

public interface TameableOps extends Tameable, org.basaltmc.bukkit.entity.Cored {
    private boolean horse() {
        return core().type.getKey().getKey().endsWith("horse") || core().type.getKey().getKey().equals("donkey")
            || core().type.getKey().getKey().equals("mule") || core().type.getKey().getKey().endsWith("llama");
    }

    default boolean isTamed() {
        return horse() ? core().flag("tamed_horse") : core().text("owner") != null;
    }

    default void setTamed(boolean tamed) {
        if (horse()) {
            core().set("tamed_horse", tamed);
        } else if (!tamed) {
            core().set("owner", null);
        }
    }

    default UUID getOwnerUniqueId() {
        String owner = core().text("owner");
        return owner == null ? null : UUID.fromString(owner);
    }

    default AnimalTamer getOwner() {
        UUID owner = getOwnerUniqueId();
        return owner == null ? null : org.bukkit.Bukkit.getOfflinePlayer(owner);
    }

    default void setOwner(AnimalTamer tamer) {
        core().set("owner", tamer == null ? null : tamer.getUniqueId().toString());
    }
}
