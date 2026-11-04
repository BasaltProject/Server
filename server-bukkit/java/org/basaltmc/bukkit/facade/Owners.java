package org.basaltmc.bukkit.facade;

import java.util.UUID;
import org.bukkit.Bukkit;
import org.bukkit.entity.AnimalTamer;

public final class Owners {
    private Owners() {
    }

    public static AnimalTamer resolve(UUID id) {
        if (Bukkit.getEntity(id) instanceof AnimalTamer tamer) {
            return tamer;
        }
        return Bukkit.getOfflinePlayer(id);
    }
}
