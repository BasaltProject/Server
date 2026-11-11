// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debugchart;

public abstract class AbstractSampleLogger implements SampleLogger {
    protected final long[] defaults;
    protected final long[] sample;

    protected AbstractSampleLogger(int dimensions, long[] defaults) {
        if (defaults.length != dimensions) {
            throw new IllegalArgumentException("defaults have incorrect length of " + defaults.length);
        }
        this.sample = new long[dimensions];
        this.defaults = defaults;
    }

    @Override
    public void logFullSample(long[] sample) {
        System.arraycopy(sample, 0, this.sample, 0, sample.length);
        flushSample();
    }

    @Override
    public void logSample(long sample) {
        this.sample[0] = sample;
        flushSample();
    }

    @Override
    public void logPartialSample(long sample, int dimension) {
        if (dimension < 1 || dimension >= this.sample.length) {
            throw new IndexOutOfBoundsException(dimension + " out of bounds for dimensions " + this.sample.length);
        }
        this.sample[dimension] = sample;
    }

    private void flushSample() {
        useSample();
        resetSample();
    }

    protected abstract void useSample();

    protected void resetSample() {
        System.arraycopy(defaults, 0, sample, 0, defaults.length);
    }
}
