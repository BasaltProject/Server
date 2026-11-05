// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.DecoderException;
import java.util.List;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

public class CompressionDecoder extends ByteToMessageDecoder {
    public static final int MAXIMUM_COMPRESSED_LENGTH = 2097152;
    public static final int MAXIMUM_UNCOMPRESSED_LENGTH = 8388608;
    private final Inflater inflater = new Inflater();
    private int threshold;
    private boolean validateDecompressed;

    public CompressionDecoder(int threshold, boolean validateDecompressed) {
        this.threshold = threshold;
        this.validateDecompressed = validateDecompressed;
    }

    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {
        if (in.readableBytes() == 0) {
            return;
        }
        int declared = VarInt.read(in);
        if (declared == 0) {
            out.add(in.readBytes(in.readableBytes()));
            return;
        }
        if (validateDecompressed) {
            if (declared < threshold) {
                throw new DecoderException("Badly compressed packet - size of " + declared + " is below server threshold of " + threshold);
            }
            if (declared > MAXIMUM_UNCOMPRESSED_LENGTH) {
                throw new DecoderException("Badly compressed packet - size of " + declared + " is larger than protocol maximum of " + MAXIMUM_UNCOMPRESSED_LENGTH);
            }
        }
        byte[] compressed = new byte[in.readableBytes()];
        in.readBytes(compressed);
        inflater.setInput(compressed);
        byte[] inflated = new byte[declared];
        try {
            int produced = inflater.inflate(inflated);
            if (produced != declared && validateDecompressed) {
                throw new DecoderException("Badly compressed packet - expected " + declared + " bytes but inflated " + produced);
            }
        } catch (DataFormatException failure) {
            throw new DecoderException("Badly compressed packet", failure);
        } finally {
            inflater.reset();
        }
        out.add(ctx.alloc().buffer(declared).writeBytes(inflated));
    }

    public void setThreshold(int threshold, boolean validateDecompressed) {
        this.threshold = threshold;
        this.validateDecompressed = validateDecompressed;
    }
}
