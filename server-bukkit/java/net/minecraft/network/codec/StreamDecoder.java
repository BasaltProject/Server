// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.codec;

public interface StreamDecoder<I, T> {
    T decode(I input);
}
