// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.Consumer;
public class AnimationState {
    private int startTick = Integer.MIN_VALUE;
    public void start(int tickCount) {
        startTick = tickCount;
    }
    public void startIfStopped(int tickCount) {
        if (!isStarted()) {
            start(tickCount);
        }
    }
    public void animateWhen(boolean condition, int tickCount) {
        if (condition) {
            startIfStopped(tickCount);
        } else {
            stop();
        }
    }
    public void stop() {
        startTick = Integer.MIN_VALUE;
    }
    public void ifStarted(Consumer<AnimationState> timer) {
        if (isStarted()) {
            timer.accept(this);
        }
    }
    public void fastForward(int ticks, float timeScale) {
        if (isStarted()) {
            startTick -= (int) (ticks * timeScale);
        }
    }
    public long getTimeInMillis(float ageInTicks) {
        return (long) ((ageInTicks - startTick) * 50.0F);
    }
    public boolean isStarted() {
        return startTick != Integer.MIN_VALUE;
    }
    public void copyFrom(AnimationState state) {
        startTick = state.startTick;
    }
}
