// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.world.phys.Vec3;
public class SteppedInterpolationTracker extends InterpolationHandler.NoOpInterpolationHandler implements InterpolationTracker {
    private final java.util.List<PositionStep> trackedSteps = new java.util.ArrayList<>();
    private int sinceLastStep = -1;
    private Vec3 predicted = Vec3.ZERO;
    private final Entity entity;
    public SteppedInterpolationTracker(Entity entity) {
        this.entity = entity;
    }
    private void record(Vec3 pos) {
        if (sinceLastStep > 0) {
            trackedSteps.add(new PositionStep(pos, sinceLastStep));
            sinceLastStep = 0;
        }
    }
    public InterpolationTracker interpolationTracker() {
        return this;
    }
    public void applyPredictedMovement(Vec3 delta) {
        this.predicted = predicted.add(delta);
    }
    public void updateTracking(Vec3 trackingPos) {
        if (predicted.lengthSqr() > 1.0E-5) {
            trackedSteps.replaceAll(step -> step.addDelta(predicted));
        }
        predicted = Vec3.ZERO;
        sinceLastStep++;
        if (entity.syncPosition) {
            entity.syncPosition = false;
            record(trackingPos);
        }
    }
    public PositionPath getPositionPath(Vec3 currentPosition) {
        record(currentPosition);
        return trackedSteps.isEmpty() ? PositionPath.of(currentPosition) : PositionPath.stepped(java.util.List.copyOf(trackedSteps));
    }
    public void clear() {
        trackedSteps.clear();
        this.sinceLastStep = 0;
    }
}
