// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public class DebugSubscription<T> {
    public static final int DOES_NOT_EXPIRE = 0;
    private final StreamCodec<? super RegistryFriendlyByteBuf, T> valueStreamCodec;
    private final int expireAfterTicks;

    public DebugSubscription(StreamCodec<? super RegistryFriendlyByteBuf, T> valueStreamCodec, int expireAfterTicks) {
        this.valueStreamCodec = valueStreamCodec;
        this.expireAfterTicks = expireAfterTicks;
    }

    public DebugSubscription(StreamCodec<? super RegistryFriendlyByteBuf, T> valueStreamCodec) {
        this(valueStreamCodec, DOES_NOT_EXPIRE);
    }

    public Update<T> packUpdate(T value) {
        return new Update<>(this, Optional.of(value));
    }

    public Update<T> emptyUpdate() {
        return new Update<>(this, Optional.empty());
    }

    public Event<T> packEvent(T value) {
        return new Event<>(this, value);
    }

    @Override
    public String toString() {
        return "DebugSubscription@" + Integer.toHexString(hashCode());
    }

    public StreamCodec<? super RegistryFriendlyByteBuf, T> valueStreamCodec() {
        return valueStreamCodec;
    }

    public int expireAfterTicks() {
        return expireAfterTicks;
    }

    public static record Update<T>(DebugSubscription<T> subscription, Optional<T> value) {
        public static final StreamCodec<RegistryFriendlyByteBuf, Update<?>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(Update.class, "STREAM_CODEC");
    }

    public static record Event<T>(DebugSubscription<T> subscription, T value) {
        public static final StreamCodec<RegistryFriendlyByteBuf, Event<?>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(Event.class, "STREAM_CODEC");
    }
}
