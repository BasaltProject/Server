// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.IOException;
import java.io.InputStream;

public class FastBufferedInputStream extends InputStream {
    private static final int kDefaultBuffer = 8192;
    private final InputStream in;
    private final byte[] buffer;
    private int limit;
    private int position;

    public FastBufferedInputStream(InputStream in) {
        this(in, kDefaultBuffer);
    }

    public FastBufferedInputStream(InputStream in, int bufferSize) {
        this.in = in;
        this.buffer = new byte[bufferSize];
    }

    private int fill() throws IOException {
        position = 0;
        limit = 0;
        int read = in.read(buffer, 0, buffer.length);
        if (read > 0) {
            limit = read;
        }
        return read;
    }

    @Override
    public int read() throws IOException {
        if (position >= limit && fill() <= 0) {
            return -1;
        }
        return buffer[position++] & 0xFF;
    }

    @Override
    public int read(byte[] output, int offset, int length) throws IOException {
        if (length == 0) {
            return 0;
        }
        if (position >= limit) {
            if (length >= buffer.length) {
                return in.read(output, offset, length);
            }
            if (fill() <= 0) {
                return -1;
            }
        }
        int count = Math.min(length, limit - position);
        System.arraycopy(buffer, position, output, offset, count);
        position += count;
        return count;
    }

    @Override
    public long skip(long count) throws IOException {
        if (count <= 0L) {
            return 0L;
        }
        long buffered = limit - position;
        if (buffered > 0L) {
            long skipped = Math.min(buffered, count);
            position += (int) skipped;
            return skipped;
        }
        return in.skip(count);
    }

    @Override
    public int available() throws IOException {
        return limit - position + in.available();
    }

    @Override
    public void close() throws IOException {
        in.close();
    }
}
