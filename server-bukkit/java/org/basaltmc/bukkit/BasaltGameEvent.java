package org.basaltmc.bukkit;

import org.bukkit.GameEvent;
import org.bukkit.NamespacedKey;

/** A game event a sculk sensor can hear. */
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
        throw Unsupported.hit("GameEvent#getRange");
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
