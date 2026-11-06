// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.Mth;

public class DensityBufferPool implements DensityBufferArena {
    private static final int kCapacityStep = 16;
    private final int maxAge;
    private final List<ScopedDensityBuffer> idle = new ArrayList<>();

    public DensityBufferPool(int maxAge) {
        this.maxAge = maxAge;
    }

    public ScopedDensityBuffer acquire(int size) {
        int wanted = Mth.roundToward(size, kCapacityStep);
        ScopedDensityBuffer reused = take(wanted, wanted * 2);
        if (reused == null) {
            return new ScopedDensityBuffer(this, wanted, size);
        }
        reused.restore(size);
        return reused;
    }

    private ScopedDensityBuffer take(int minCapacity, int maxCapacity) {
        int best = -1;
        int bestCapacity = maxCapacity + 1;
        for (int i = idle.size() - 1; i >= 0; i--) {
            int capacity = idle.get(i).capacity();
            if (capacity == minCapacity) {
                return idle.remove(i);
            }
            if (capacity > minCapacity && capacity < bestCapacity) {
                best = i;
                bestCapacity = capacity;
            }
        }
        return best == -1 ? null : idle.remove(best);
    }

    public void release(ScopedDensityBuffer buffer) {
        idle.add(buffer);
    }

    public void garbageCollect() {
        idle.removeIf(buffer -> buffer.incrementAge() > maxAge);
    }

    public void clear() {
        idle.clear();
    }

    int size() {
        return idle.size();
    }

    public boolean isEmpty() {
        return idle.isEmpty();
    }
}
