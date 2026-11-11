// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debugchart;

public class LocalSampleLogger extends AbstractSampleLogger implements SampleStorage {
    public static final int CAPACITY = 240;
    private final long[][] samples;
    private int start;
    private int size;

    public LocalSampleLogger(int dimensions) {
        this(dimensions, new long[dimensions]);
    }

    public LocalSampleLogger(int dimensions, long[] defaults) {
        super(dimensions, defaults);
        this.samples = new long[CAPACITY][dimensions];
    }

    @Override
    protected void useSample() {
        int slot = (start + size) % CAPACITY;
        System.arraycopy(sample, 0, samples[slot], 0, sample.length);
        if (size < CAPACITY) {
            size++;
        } else {
            start = (start + 1) % CAPACITY;
        }
    }

    @Override
    public int capacity() {
        return CAPACITY;
    }

    @Override
    public int size() {
        return size;
    }

    @Override
    public long get(int index) {
        return get(index, 0);
    }

    @Override
    public long get(int index, int dimension) {
        if (index < 0 || index >= size) {
            throw new IndexOutOfBoundsException(index + " out of bounds for length " + size);
        }
        return samples[(start + index) % CAPACITY][dimension];
    }

    @Override
    public void reset() {
        start = 0;
        size = 0;
    }
}
