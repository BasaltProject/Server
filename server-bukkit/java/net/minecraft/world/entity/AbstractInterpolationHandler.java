// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractInterpolationHandler implements InterpolationHandler {
    protected final Entity entity;
    protected int interpolationSteps;
    protected final PositionAndRotation.Mutable lastPositionAndRotation = new PositionAndRotation.Mutable();
    public AbstractInterpolationHandler(Entity entity, int interpolationSteps) {
        this.entity = entity;
        this.interpolationSteps = interpolationSteps;
    }
    private void shiftTarget(Vec3 delta) {
        Vec3 moved = interpolationData().position().add(delta);
        if (entity.level().noCollision(entity, entity.makeBoundingBox(moved))) {
            interpolationData().addDelta(delta);
        }
    }
    private void rememberPose() {
        lastPositionAndRotation.set(entity.position(), entity.getYRot(), entity.getXRot());
    }
    protected abstract PositionAndRotation.Mutable interpolationData();
    protected abstract void startInterpolating(PositionPath position, float yRot, float xRot);
    protected abstract void doInterpolate();
    public PositionAndRotation target() {
        return hasActiveInterpolation() ? interpolationData().immutable() : null;
    }
    public boolean interpolateTo(PositionPath position, float yRot, float xRot, boolean hasRotation) {
        PositionAndRotation basis = hasActiveInterpolation() ? interpolationData() : entity.storePositionAndRotation();
        interpolateTo(position != null ? position : PositionPath.of(basis.position()),
            hasRotation ? yRot : basis.yRot(), hasRotation ? xRot : basis.xRot());
        return true;
    }
    protected void interpolateTo(PositionPath position, float yRot, float xRot) {
        if (interpolationSteps == 0) {
            entity.snapTo(position.endPosition(), yRot, xRot);
            cancel();
            return;
        }
        if (hasActiveInterpolation() && interpolationData().is(position.endPosition(), yRot, xRot)) {
            return;
        }
        startInterpolating(position, yRot, xRot);
        rememberPose();
        entity.onInterpolationStart(this);
    }
    public void applyPredictedMovement(Vec3 delta) {
        shiftTarget(delta);
        lastPositionAndRotation.addDelta(delta);
    }
    public void interpolate() {
        if (!hasActiveInterpolation()) {
            cancel();
            return;
        }
        Vec3 moved = entity.position().subtract(lastPositionAndRotation.position());
        if (moved.lengthSqr() > 1.0E-5) {
            shiftTarget(moved);
        }
        interpolationData().addRotation(entity.getYRot() - lastPositionAndRotation.yRot(), entity.getXRot() - lastPositionAndRotation.xRot());
        doInterpolate();
        rememberPose();
    }
}
