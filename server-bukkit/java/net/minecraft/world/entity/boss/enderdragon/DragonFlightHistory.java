// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon;
import net.minecraft.util.Mth;
public class DragonFlightHistory {
    private final Sample[] samples = new Sample[64];
    private int head = -1;
    public static final int LENGTH = 64;
    public DragonFlightHistory() {
        
    }
    public void copyFrom(DragonFlightHistory history) {
        System.arraycopy(history.samples, 0, samples, 0, LENGTH);
        this.head = history.head;
    }
    public void record(double y, float yRot) {
        Sample fresh = new Sample(y, yRot);
        if (head < 0) {
            java.util.Arrays.fill(samples, fresh);
        }
        head = (head + 1) % LENGTH;
        samples[head] = fresh;
    }
    public Sample get(int delay) {
        return samples[(head - delay) & (LENGTH - 1)];
    }
    public Sample get(int delay, float partialTicks) {
        Sample now = get(delay);
        Sample before = get(delay + 1);
        return new Sample(Mth.lerp((double) partialTicks, before.y(), now.y()), Mth.rotLerp(partialTicks, before.yRot(), now.yRot()));
    }
    public static record Sample(double y, float yRot) {
    }
}
