// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.util.Mth;
import net.minecraft.core.PositionAndRotation;
public class LinearInterpolationHandler extends AbstractInterpolationHandler {
    private final InterpolationData data = new InterpolationData();
    public static final int DEFAULT_INTERPOLATION_STEPS = 3;
    private LinearInterpolationHandler(Entity entity, int interpolationSteps) {
        super(entity, interpolationSteps);
    }
    public static InterpolationHandler create(Entity entity, int interpolationSteps) {
        return entity.level().isClientSide() ? new LinearInterpolationHandler(entity, interpolationSteps) : InterpolationHandler.NO_OP;
    }
    public static InterpolationHandler create(Entity entity) {
        return create(entity, DEFAULT_INTERPOLATION_STEPS);
    }
    public void setInterpolationLength(int steps) {
        this.interpolationSteps = steps;
    }
    protected PositionAndRotation.Mutable interpolationData() {
        return data;
    }
    protected void startInterpolating(PositionPath position, float yRot, float xRot) {
        data.set(position.endPosition(), yRot, xRot);
        data.remainingSteps = interpolationSteps;
    }
    protected void doInterpolate() {
        double alpha = 1.0 / data.remainingSteps;
        entity.setPos(entity.position().lerp(data.position(), alpha));
        entity.setRot((float) Mth.rotLerp(alpha, entity.getYRot(), data.yRot()), (float) Mth.lerp(alpha, entity.getXRot(), data.xRot()));
        data.remainingSteps--;
    }
    public boolean hasActiveInterpolation() {
        return data.remainingSteps > 0;
    }
    public void cancel() {
        data.remainingSteps = 0;
    }
    static class InterpolationData extends PositionAndRotation.Mutable {
        private int remainingSteps;
    }
}
