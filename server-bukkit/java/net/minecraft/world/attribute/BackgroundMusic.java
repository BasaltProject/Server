// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.sounds.Music;
import net.minecraft.sounds.SoundEvent;

public record BackgroundMusic(Optional<Music> defaultMusic, Optional<Music> creativeMusic, Optional<Music> underwaterMusic) {
    public static final BackgroundMusic EMPTY = new BackgroundMusic(Optional.empty(), Optional.empty(), Optional.empty());
    public static final BackgroundMusic OVERWORLD = EMPTY;
    public static final Codec<BackgroundMusic> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(BackgroundMusic.class, "CODEC");

    public BackgroundMusic(Music music) {
        this(Optional.of(music), Optional.empty(), Optional.empty());
    }

    public BackgroundMusic(Holder<SoundEvent> sound) {
        this(new Music(sound, 12000, 24000, false));
    }

    public BackgroundMusic withUnderwater(Music underwaterMusic) {
        return new BackgroundMusic(defaultMusic, creativeMusic, Optional.of(underwaterMusic));
    }

    public Optional<Music> select(boolean isCreative, boolean isUnderwater) {
        if (isUnderwater && underwaterMusic.isPresent()) {
            return underwaterMusic;
        }
        return isCreative && creativeMusic.isPresent() ? creativeMusic : defaultMusic;
    }
}
