// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.world.phys.Vec3;
public interface InterpolationHandler {
    InterpolationHandler NO_OP = new NoOpInterpolationHandler();
    default InterpolationTracker interpolationTracker() {
        return InterpolationTracker.NO_OP;
    }
    PositionAndRotation target();
    boolean interpolateTo(PositionPath position, float yRot, float xRot, boolean hasRotation);
    void interpolate();
    void applyPredictedMovement(Vec3 delta);
    boolean hasActiveInterpolation();
    void cancel();
    public static class NoOpInterpolationHandler implements InterpolationHandler {
        public PositionAndRotation target() {
            return null;
        }
        public boolean interpolateTo(PositionPath position, float yRot, float xRot, boolean hasRotation) {
            return false;
        }
        public void interpolate() {
        }
        public void applyPredictedMovement(Vec3 delta) {
        }
        public boolean hasActiveInterpolation() {
            return false;
        }
        public void cancel() {
        }
    }
}
