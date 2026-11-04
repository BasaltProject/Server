package org.basaltmc.bukkit.net;

import java.util.function.Supplier;
import net.minecraft.network.codec.StreamCodec;

/** Builds a packet codec lazily-safe: a codec that cannot be assembled becomes one that reports why it is unusable. */
public final class GuardedCodecs {
    private GuardedCodecs() {}

    public static <B, V> StreamCodec<B, V> guard(String name, Supplier<StreamCodec<B, V>> factory) {
        try {
            StreamCodec<B, V> codec = factory.get();
            if (codec != null) {
                return codec;
            }
        } catch (RuntimeException | LinkageError failure) {
            return unusable(name, failure);
        }
        return unusable(name, null);
    }

    public static <B, V> StreamCodec<B, V> encodeOnly(String name, net.minecraft.network.codec.StreamEncoder<B, V> encoder) {
        return StreamCodec.of(encoder, buf -> {
            throw new UnsupportedOperationException(name + " can only be written by Basalt");
        });
    }

    private static <B, V> StreamCodec<B, V> unusable(String name, Throwable cause) {
        return StreamCodec.of(
            (buf, value) -> {
                throw new UnsupportedOperationException(name + " is not assembled by Basalt: " + cause, cause);
            },
            buf -> {
                throw new UnsupportedOperationException(name + " is not assembled by Basalt: " + cause, cause);
            });
    }
}
