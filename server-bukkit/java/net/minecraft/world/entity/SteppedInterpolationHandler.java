// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.util.Mth;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.world.phys.Vec3;
public class SteppedInterpolationHandler extends AbstractInterpolationHandler {
    private final InterpolationData data = new InterpolationData();
    private SteppedInterpolationHandler(Entity entity) {
        super(entity, entity.getType().updateInterval());
    }
    public static InterpolationHandler create(Entity entity) {
        return entity.level().isClientSide() ? new SteppedInterpolationHandler(entity) : new SteppedInterpolationTracker(entity);
    }
    protected PositionAndRotation.Mutable interpolationData() {
        return data;
    }
    protected void startInterpolating(PositionPath position, float yRot, float xRot) {
        if (!hasActiveInterpolation()) {
            data.setStartingPoint(entity.position(), entity.getYRot(), entity.getXRot());
        }
        Vec3 end = position.endPosition();
        if (end.equals(data.position())) {
            data.addStep(end, yRot, xRot, interpolationSteps);
        } else {
            data.addSteps(position, yRot, xRot, interpolationSteps);
        }
        data.set(end, yRot, xRot);
    }
    protected void doInterpolate() {
        PositionAndRotation next = data.currentPose();
        entity.setPos(next.position());
        entity.setRot(next.yRot(), next.xRot());
        data.advance(entity.level().getRelativeTickSpeed(), interpolationSteps);
    }
    public boolean hasActiveInterpolation() {
        return !data.steps.isEmpty();
    }
    public void cancel() {
        data.reset();
    }
    static class Step extends PositionAndRotation.Mutable {
        private final int tickOffset;
        public Step(Vec3 position, float yRot, float xRot, int tickOffset) {
            this.tickOffset = tickOffset;
            set(position, yRot, xRot);
            
        }
    }
    static class InterpolationData extends PositionAndRotation.Mutable {
        private final java.util.LinkedList<Step> steps = new java.util.LinkedList<>();
        private final PositionAndRotation.Mutable lastStep = new PositionAndRotation.Mutable();
        private float elapsed;
        private float pending;
        private float speed = 1.0F;
        private void advance(float ticks, int total) {
            speed = Mth.lerp(1.0F / total, speed, Math.max(pending / total, 1.0F));
            float used = ticks * speed < pending ? ticks * speed : pending;
            if (used == pending) {
                speed = 1.0F;
            }
            elapsed += used;
            pending -= used;
        }
        private void reset() {
            steps.clear();
            pending = 0.0F;
            speed = 1.0F;
        }
        private void setStartingPoint(Vec3 position, float yRot, float xRot) {
            lastStep.set(position, yRot, xRot);
            elapsed = 1.0F;
        }
        private void addStep(Vec3 position, float yRot, float xRot, int ticks) {
            steps.add(new Step(position, yRot, xRot, ticks));
            pending += ticks;
        }
        private void addSteps(PositionPath path, float yRot, float xRot, int ticks) {
            if (path instanceof PositionPath.Linear linear) {
                addStep(linear.endPosition(), yRot, xRot, ticks);
            } else if (path instanceof PositionPath.Stepped stepped) {
                addStepped(stepped.steps(), yRot, xRot);
            }
        }
        private void addStepped(java.util.List<PositionStep> list, float yRot, float xRot) {
            if (yRot == yRot() && xRot == xRot()) {
                for (PositionStep each : list) {
                    addStep(each.position(), yRot, xRot, each.tickOffset());
                }
                return;
            }
            int total = 0;
            for (PositionStep each : list) {
                total += each.tickOffset();
            }
            int passed = 0;
            for (PositionStep each : list) {
                passed += each.tickOffset();
                float a = (float) passed / total;
                addStep(each.position(), Mth.rotLerp(a, yRot(), yRot), Mth.lerp(a, xRot(), xRot), each.tickOffset());
            }
        }
        private PositionAndRotation currentPose() {
            while (!steps.isEmpty()) {
                Step head = steps.getFirst();
                if (elapsed < head.tickOffset) {
                    float a = elapsed / head.tickOffset;
                    return PositionAndRotation.of(lastStep.position().lerp(head.position(), a), Mth.rotLerp(a, lastStep.yRot(), head.yRot()), Mth.lerp(a, lastStep.xRot(), head.xRot()));
                }
                elapsed -= head.tickOffset;
                lastStep.set(head.position(), head.yRot(), head.xRot());
                steps.removeFirst();
            }
            return this;
        }
        public void addDelta(Vec3 delta) {
            super.addDelta(delta);
            for (Step each : steps) {
                each.addDelta(delta);
            }
            lastStep.addDelta(delta);
        }
        public void addRotation(float yRot, float xRot) {
            super.addRotation(yRot, xRot);
            for (Step each : steps) {
                each.addRotation(yRot, xRot);
            }
            lastStep.addRotation(yRot, xRot);
        }
    }
}
