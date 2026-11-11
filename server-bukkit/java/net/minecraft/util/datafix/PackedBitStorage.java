// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix;
import net.minecraft.util.Mth;
import org.apache.commons.lang3.Validate;
public class PackedBitStorage {
    private final long[] words;
    private final int bitsPerEntry;
    private final long entryMask;
    private final int length;
    public PackedBitStorage(int bits, int size) {
        this(bits, size, new long[wordsFor(size, bits)]);
    }
    public PackedBitStorage(int bits, int size, long[] data) {
        Validate.inclusiveBetween(1L, 32L, (long) bits);
        this.length = size;
        this.bitsPerEntry = bits;
        this.words = data;
        this.entryMask = (1L << bits) - 1L;
        int expected = wordsFor(size, bits);
        if (data.length != expected) {
            throw new IllegalArgumentException("Invalid length given for storage, got: " + data.length + " but expected: " + expected);
        }
    }
    private static int wordsFor(int size, int bits) {
        return Mth.roundToward(size * bits, 64) / 64;
    }
    public void set(int index, int value) {
        Validate.inclusiveBetween(0L, (long) (length - 1), (long) index);
        Validate.inclusiveBetween(0L, entryMask, (long) value);
        int first = (index * bitsPerEntry) >> 6;
        int last = ((index + 1) * bitsPerEntry - 1) >> 6;
        int offset = (index * bitsPerEntry) & 63;
        long bits = (long) value & entryMask;
        words[first] = (words[first] & ~(entryMask << offset)) | (bits << offset);
        if (first != last) {
            int spilled = bitsPerEntry - (64 - offset);
            words[last] = ((words[last] >>> spilled) << spilled) | (bits >> (64 - offset));
        }
    }
    public int get(int index) {
        Validate.inclusiveBetween(0L, (long) (length - 1), (long) index);
        int first = (index * bitsPerEntry) >> 6;
        int last = ((index + 1) * bitsPerEntry - 1) >> 6;
        int offset = (index * bitsPerEntry) & 63;
        long raw = first == last ? words[first] >>> offset : (words[first] >>> offset) | (words[last] << (64 - offset));
        return (int) (raw & entryMask);
    }
    public long[] getRaw() {
        return words;
    }
    public int getBits() {
        return bitsPerEntry;
    }
}
