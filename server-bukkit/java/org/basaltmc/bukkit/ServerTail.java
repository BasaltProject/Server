package org.basaltmc.bukkit;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import org.bukkit.World;
import org.bukkit.plugin.Plugin;
import org.bukkit.util.CachedServerIcon;

public abstract class ServerTail extends ServerLeftovers {
    private final Set<String> pauseVetoes = ConcurrentHashMap.newKeySet();

    private static int number(String key) {
        String value = WorldEnvNatives.setting(key);
        return value == null ? 0 : Integer.parseInt(value);
    }

    @Override
    public long[] getTickTimes() {
        return RestNatives.tickTimes();
    }

    @Override
    public boolean isPaused() {
        return RestNatives.paused();
    }

    @Override
    public int getPauseWhenEmptyTime() {
        return number("pause_when_empty_seconds");
    }

    @Override
    public void setPauseWhenEmptyTime(int seconds) {
        if (seconds < 0) {
            throw new IllegalArgumentException("Seconds cannot be negative");
        }
        WorldEnvNatives.setSetting("pause_when_empty_seconds", Integer.toString(seconds));
    }

    @Override
    public void allowPausing(Plugin plugin, boolean value) {
        if (value) {
            pauseVetoes.remove(plugin.getName());
        } else {
            pauseVetoes.add(plugin.getName());
        }
        WorldEnvNatives.setSetting("pause_allowed", Boolean.toString(pauseVetoes.isEmpty()));
    }

    @Override
    public int getSpawnRadius() {
        return number("spawn_protection");
    }

    @Override
    public void setSpawnRadius(int radius) {
        WorldEnvNatives.setSetting("spawn_protection", Integer.toString(Math.max(0, radius)));
    }

    @Override
    public World getRespawnWorld() {
        return getWorld(WorldEnvNatives.setting("respawn_world"));
    }

    @Override
    public void setRespawnWorld(World world) {
        if (world == null) {
            throw new IllegalArgumentException("world cannot be null");
        }
        if (!WorldEnvNatives.setSetting("respawn_world", world.getName())) {
            throw new IllegalArgumentException("The world " + world.getName() + " is not loaded");
        }
    }

    @Override
    public CachedServerIcon getServerIcon() {
        byte[] png = RestNatives.serverIcon();
        return png == null || png.length == 0 ? ServerIcon.empty() : ServerIcon.of(png);
    }

    @Override
    public void updateRecipes() {
        for (org.bukkit.entity.Player player : getOnlinePlayers()) {
            org.basaltmc.bukkit.player.PlayerCalls.call(player.getUniqueId(), "resend_recipes");
        }
    }

    @Override
    public boolean reloadCommandAliases() {
        return RestNatives.reloadCommands();
    }
}
