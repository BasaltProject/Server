package net.minecraft.network;

import io.netty.buffer.ByteBuf;

/** Present so plugins that probe the vanilla buffer class by reflection find what they look for. */
public class FriendlyByteBuf {
    private final ByteBuf source;

    public FriendlyByteBuf(ByteBuf source) {
        this.source = source;
    }

    public ByteBuf source() {
        return source;
    }
}
