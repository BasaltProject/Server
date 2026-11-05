// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.sounds.SoundEvent;

public record AmbientSounds(Optional<Holder<SoundEvent>> loop, Optional<AmbientMoodSettings> mood, List<AmbientAdditionsSettings> additions) {
    public static final AmbientSounds EMPTY = new AmbientSounds(Optional.empty(), Optional.empty(), List.of());
    public static final AmbientSounds LEGACY_CAVE_SETTINGS = EMPTY;
    public static final Codec<AmbientSounds> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(AmbientSounds.class, "CODEC");
}
