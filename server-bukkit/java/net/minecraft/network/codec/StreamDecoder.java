package net.minecraft.network.codec;

public interface StreamDecoder<I, T> {
    T decode(I input);
}
