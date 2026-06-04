package net.minecraft.network.codec;

public interface StreamEncoder<O, T> {
    void encode(O output, T value);
}
