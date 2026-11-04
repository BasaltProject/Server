// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
public class TickThrottler {
    private final int incrementStep;
    private final int threshold;
    private int count;
    public TickThrottler(int incrementStep, int threshold) {
        this.incrementStep = incrementStep;
        this.threshold = threshold;
    }
    public void increment() {
        count += incrementStep;
    }
    public void tick() {
        if (count > 0) {
            count--;
        }
    }
    public boolean isUnderThreshold() {
        return threshold <= 0 || count < threshold;
    }
}
