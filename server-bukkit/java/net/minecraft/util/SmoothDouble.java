// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
public class SmoothDouble {
    private double targetValue;
    private double remainingValue;
    private double lastAmount;
    public double getNewDeltaValue(double targetDelta, double time) {
        targetValue += targetDelta;
        double delta = targetValue - remainingValue;
        double blended = Mth.lerp(0.5, lastAmount, delta);
        double sign = Math.signum(delta);
        if (sign * delta > sign * lastAmount) {
            delta = blended;
        }
        lastAmount = blended;
        remainingValue += delta * time;
        return delta * time;
    }
    public void reset() {
        targetValue = 0.0;
        remainingValue = 0.0;
        lastAmount = 0.0;
    }
}
