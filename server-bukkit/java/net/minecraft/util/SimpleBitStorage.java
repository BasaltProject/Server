package net.minecraft.util;

import java.util.function.IntConsumer;

/** Entries never straddle a long, as in the world format. */
public class SimpleBitStorage implements BitStorage {
    private final long[] data;
    private final int bits;
    private final long mask;
    private final int size;
    private final int valuesPerLong;

    public SimpleBitStorage(int bits, int size, int[] values) {
        this(bits, size);
        int out = 0;
        int offset = 0;
        for (; offset <= size - valuesPerLong; offset += valuesPerLong) {
            long packed = 0L;
            for (int at = valuesPerLong - 1; at >= 0; at--) {
                packed = (packed << bits) | (values[offset + at] & mask);
            }
            data[out++] = packed;
        }
        int rest = size - offset;
        if (rest > 0) {
            long packed = 0L;
            for (int at = rest - 1; at >= 0; at--) {
                packed = (packed << bits) | (values[offset + at] & mask);
            }
            data[out] = packed;
        }
    }

    public SimpleBitStorage(int bits, int size) {
        this(bits, size, (long[]) null);
    }

    public SimpleBitStorage(int bits, int size, long[] data) {
        if (bits < 1 || bits > 32) {
            throw new IllegalArgumentException("bits " + bits);
        }
        this.size = size;
        this.bits = bits;
        this.mask = (1L << bits) - 1L;
        this.valuesPerLong = 64 / bits;
        int required = (size + valuesPerLong - 1) / valuesPerLong;
        if (data != null) {
            if (data.length != required) {
                throw new InitializationException("Invalid length given for storage, got: " + data.length
                    + " but expected: " + required);
            }
            this.data = data;
        } else {
            this.data = new long[required];
        }
    }

    @Override
    public int getAndSet(int index, int value) {
        int cell = index / valuesPerLong;
        long held = data[cell];
        int shift = (index - cell * valuesPerLong) * bits;
        int old = (int) (held >> shift & mask);
        data[cell] = held & ~(mask << shift) | ((long) value & mask) << shift;
        return old;
    }

    @Override
    public void set(int index, int value) {
        int cell = index / valuesPerLong;
        long held = data[cell];
        int shift = (index - cell * valuesPerLong) * bits;
        data[cell] = held & ~(mask << shift) | ((long) value & mask) << shift;
    }

    @Override
    public int get(int index) {
        int cell = index / valuesPerLong;
        return (int) (data[cell] >> (index - cell * valuesPerLong) * bits & mask);
    }

    public void fill(int value) {
        long cell = (long) value & mask;
        int offset = bits;
        do {
            cell |= cell << offset;
            offset <<= 1;
        } while (offset < 64);
        java.util.Arrays.fill(data, cell);
    }

    @Override
    public long[] getRaw() {
        return data;
    }

    @Override
    public int getSize() {
        return size;
    }

    @Override
    public int getBits() {
        return bits;
    }

    @Override
    public void getAll(IntConsumer output) {
        int count = 0;
        for (long cell : data) {
            for (int i = 0; i < valuesPerLong; i++) {
                output.accept((int) (cell & mask));
                cell >>= bits;
                if (++count >= size) {
                    return;
                }
            }
        }
    }

    @Override
    public void unpack(int[] output) {
        int count = 0;
        for (long cell : data) {
            for (int i = 0; i < valuesPerLong; i++) {
                output[count] = (int) (cell & mask);
                cell >>= bits;
                if (++count >= size) {
                    return;
                }
            }
        }
    }

    @Override
    public BitStorage copy() {
        return new SimpleBitStorage(bits, size, data.clone());
    }

    public static class InitializationException extends RuntimeException {
        public InitializationException(String message) {
            super(message);
        }
    }
}
