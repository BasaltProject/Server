// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.codec;

public interface StreamEncoder<O, T> {
    void encode(O output, T value);
}
