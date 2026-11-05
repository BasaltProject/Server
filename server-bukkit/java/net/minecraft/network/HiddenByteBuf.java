// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.util.ReferenceCounted;

public record HiddenByteBuf(ByteBuf contents) implements ReferenceCounted {
    public HiddenByteBuf {
        java.util.Objects.requireNonNull(contents);
    }

    public static Object pack(Object msg) {
        return msg instanceof ByteBuf buffer ? new HiddenByteBuf(buffer) : msg;
    }

    public static Object unpack(Object msg) {
        return msg instanceof HiddenByteBuf hidden ? hidden.contents : msg;
    }

    public int refCnt() {
        return contents.refCnt();
    }

    public HiddenByteBuf retain() {
        contents.retain();
        return this;
    }

    public HiddenByteBuf retain(int increment) {
        contents.retain(increment);
        return this;
    }

    public HiddenByteBuf touch() {
        contents.touch();
        return this;
    }

    public HiddenByteBuf touch(Object hint) {
        contents.touch(hint);
        return this;
    }

    public boolean release() {
        return contents.release();
    }

    public boolean release(int decrement) {
        return contents.release(decrement);
    }
}
