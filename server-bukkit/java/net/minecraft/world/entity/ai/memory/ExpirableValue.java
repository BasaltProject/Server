// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.memory;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;

public record ExpirableValue<T>(T value, Optional<Long> timeToLive) {
    public static <T> ExpirableValue<T> of(T value) {
        return new ExpirableValue<>(value, Optional.empty());
    }

    public static <T> ExpirableValue<T> of(T value, long ticksUntilExpiry) {
        return new ExpirableValue<>(value, Optional.of(ticksUntilExpiry));
    }

    @Override
    public String toString() {
        return value + timeToLive.map(ttl -> " (ttl: " + ttl + ")").orElse("");
    }

    public static <T> Codec<ExpirableValue<T>> codec(Codec<T> valueCodec) {
        return RecordCodecBuilder.create(instance -> instance.group(
            valueCodec.fieldOf("value").forGetter(ExpirableValue::value),
            Codec.LONG.lenientOptionalFieldOf("ttl").forGetter(ExpirableValue::timeToLive)).apply(instance, ExpirableValue::new));
    }
}
