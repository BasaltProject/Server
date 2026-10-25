package org.basaltmc.bukkit;

import java.util.List;
import net.kyori.adventure.text.Component;
import org.bukkit.MusicInstrument;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Sound;

/** A goat horn call by the name the game gives it. */
final class BasaltMusicInstrument extends MusicInstrument {
    static final List<String> kNames = List.of("ponder_goat_horn", "sing_goat_horn", "seek_goat_horn",
        "feel_goat_horn", "admire_goat_horn", "call_goat_horn", "yearn_goat_horn", "dream_goat_horn");

    private final NamespacedKey key;

    BasaltMusicInstrument(NamespacedKey key) {
        this.key = key;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public float getDuration() {
        return 7.0F;
    }

    @Override
    public float getRange() {
        return 256.0F;
    }

    @Override
    public Component description() {
        return Component.translatable(translationKey());
    }

    @Override
    public Sound getSound() {
        int at = Math.max(0, kNames.indexOf(key.getKey()));
        return Registry.SOUNDS.get(NamespacedKey.minecraft("item.goat_horn.sound." + at));
    }

    @Override
    public String translationKey() {
        return "instrument." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public String toString() {
        return "MusicInstrument{" + key + "}";
    }
}
