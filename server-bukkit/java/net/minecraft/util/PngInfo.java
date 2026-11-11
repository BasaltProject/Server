// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public record PngInfo(int width, int height, byte bitDepth, byte colorType) {
    private static final long kSignature = 0x89504E470D0A1A0AL;
    private static final int kIhdr = 0x49484452;

    public static PngInfo fromStream(InputStream inputStream) throws IOException {
        return fromBytes(inputStream.readNBytes(33));
    }

    public static PngInfo fromBytes(byte[] bytes) throws IOException {
        ByteBuffer buffer = ByteBuffer.wrap(bytes);
        validateHeader(buffer);
        return new PngInfo(buffer.getInt(), buffer.getInt(), buffer.get(), buffer.get());
    }

    public static void validateHeader(ByteBuffer buffer) throws IOException {
        if (buffer.remaining() < 8 + 8 + 13 || buffer.getLong() != kSignature) {
            throw new IOException("Bad PNG Signature");
        }
        if (buffer.getInt() != 13) {
            throw new IOException("Bad length for IHDR chunk!");
        }
        if (buffer.getInt() != kIhdr) {
            throw new IOException("Bad type for IHDR chunk!");
        }
    }
}
