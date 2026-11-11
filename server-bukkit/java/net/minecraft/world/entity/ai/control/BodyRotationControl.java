// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.control;
import net.minecraft.world.entity.Mob;
import net.minecraft.util.Mth;
public class BodyRotationControl implements Control {
    private static final float kStableAngle = 15.0F;
    private static final int kFaceForwardDelay = 10;
    private static final int kFaceForwardTicks = 10;
    private final Mob mob;
    private int headStableTime;
    private float lastStableYHeadRot;
    public BodyRotationControl(Mob mob) {
        this.mob = mob;
    }
    public void clientTick() {
        if (moving()) {
            mob.yBodyRot = mob.getYRot();
            mob.yHeadRot = Mth.rotateIfNecessary(mob.yHeadRot, mob.yBodyRot, (float) mob.getMaxHeadYRot());
            lastStableYHeadRot = mob.yHeadRot;
            headStableTime = 0;
            return;
        }
        if (mob.getFirstPassenger() instanceof Mob) {
            return;
        }
        if (Math.abs(mob.yHeadRot - lastStableYHeadRot) > kStableAngle) {
            headStableTime = 0;
            lastStableYHeadRot = mob.yHeadRot;
            mob.yBodyRot = Mth.rotateIfNecessary(mob.yBodyRot, mob.yHeadRot, (float) mob.getMaxHeadYRot());
        } else if (++headStableTime > kFaceForwardDelay) {
            float fraction = Mth.clamp((headStableTime - kFaceForwardDelay) / (float) kFaceForwardTicks, 0.0F, 1.0F);
            mob.yBodyRot = Mth.rotateIfNecessary(mob.yBodyRot, mob.yHeadRot, mob.getMaxHeadYRot() * (1.0F - fraction));
        }
    }
    private boolean moving() {
        double dx = mob.getX() - mob.xo;
        double dz = mob.getZ() - mob.zo;
        return dx * dx + dz * dz > 2.5000003E-7F;
    }
}
