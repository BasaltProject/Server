// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import javax.crypto.Cipher;
import javax.crypto.ShortBufferException;

public class CipherBase {
    private final Cipher cipher;
    private byte[] heapIn = new byte[0];
    private byte[] heapOut = new byte[0];

    protected CipherBase(Cipher cipher) {
        this.cipher = cipher;
    }

    private byte[] bytesOf(ByteBuf buffer) {
        int length = buffer.readableBytes();
        if (heapIn.length < length) {
            heapIn = new byte[length];
        }
        buffer.readBytes(heapIn, 0, length);
        return heapIn;
    }

    protected ByteBuf decipher(ChannelHandlerContext ctx, ByteBuf in) throws ShortBufferException {
        int length = in.readableBytes();
        byte[] source = bytesOf(in);
        ByteBuf result = ctx.alloc().heapBuffer(cipher.getOutputSize(length));
        result.writerIndex(cipher.update(source, 0, length, result.array(), result.arrayOffset()));
        return result;
    }

    protected void encipher(ByteBuf in, ByteBuf out) throws ShortBufferException {
        int length = in.readableBytes();
        byte[] source = bytesOf(in);
        int capacity = cipher.getOutputSize(length);
        if (heapOut.length < capacity) {
            heapOut = new byte[capacity];
        }
        out.writeBytes(heapOut, 0, cipher.update(source, 0, length, heapOut));
    }
}
