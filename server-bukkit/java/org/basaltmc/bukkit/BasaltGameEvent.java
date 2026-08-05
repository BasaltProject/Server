package org.basaltmc.bukkit;

import org.bukkit.GameEvent;
import org.bukkit.NamespacedKey;

final class BasaltGameEvent extends GameEvent {
    private final NamespacedKey key;

    BasaltGameEvent(NamespacedKey key) {
        this.key = key;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public int getRange() {
        return switch (key.getKey()) {
            case "jukebox_play", "jukebox_stop_play" -> 10;
            case "shriek" -> 32;
            default -> 16;
        };
    }

    @Override
    public int getVibrationLevel() {
        return WorldEnvNatives.gameEventFrequency(key.getKey());
    }

    @Override
    public String toString() {
        return "GameEvent{" + key + "}";
    }
}
