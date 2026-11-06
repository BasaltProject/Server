// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import java.util.Arrays;

public class DensityBuffer {
    protected final float[] values;
    protected int size;

    protected DensityBuffer(int size) {
        this.values = new float[size];
        this.size = size;
    }

    public static DensityBuffer createUnpooled(int size) {
        return new DensityBuffer(size);
    }

    public void set(int index, float value) {
        values[index] = value;
    }

    public void setRange(int index, int size, float value) {
        Arrays.fill(values, index, index + size, value);
    }

    public void addTo(int index, float value) {
        values[index] += value;
    }

    public float get(int index) {
        return values[index];
    }

    public void fill(float value) {
        Arrays.fill(values, 0, size, value);
    }

    public void copyFrom(DensityBuffer other) {
        if (other.size != size) {
            throw new IllegalArgumentException("Cannot copy a buffer of size " + other.size + " into one of size " + size);
        }
        System.arraycopy(other.values, 0, values, 0, size);
    }

    public int capacity() {
        return values.length;
    }

    public int size() {
        return size;
    }
}
