// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

public class BinaryAnimator {
    private final int animationLength;
    private final EasingType easing;
    private int ticks;
    private int ticksOld;

    public BinaryAnimator(int animationLength, EasingType easing) {
        this.animationLength = animationLength;
        this.easing = easing;
    }

    public BinaryAnimator(int animationLength) {
        this(animationLength, EasingType.LINEAR);
    }

    public void tick(boolean active) {
        ticksOld = ticks;
        ticks = Mth.clamp(ticks + (active ? 1 : -1), 0, animationLength);
    }

    public float getFactor(float partialTicks) {
        float progress = Mth.lerp(partialTicks, (float) ticksOld, (float) ticks) / (float) animationLength;
        return easing.apply(progress);
    }
}
