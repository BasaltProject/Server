// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.control;

import java.util.Optional;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.phys.Vec3;

public class LookControl implements Control {
    protected final Mob mob;
    protected float yMaxRotSpeed;
    protected float xMaxRotAngle;
    protected int lookAtCooldown;
    protected double wantedX;
    protected double wantedY;
    protected double wantedZ;

    public LookControl(Mob mob) {
        this.mob = mob;
    }

    public void setLookAt(Vec3 vec) {
        setLookAt(vec.x, vec.y, vec.z);
    }

    public void setLookAt(Entity target) {
        setLookAt(target.getX(), target.getEyeY(), target.getZ());
    }

    public void setLookAt(Entity target, float yMaxRotSpeed, float xMaxRotAngle) {
        setLookAt(target.getX(), target.getEyeY(), target.getZ(), yMaxRotSpeed, xMaxRotAngle);
    }

    public void setLookAt(double x, double y, double z) {
        setLookAt(x, y, z, mob.getHeadRotSpeed(), mob.getMaxHeadXRot());
    }

    public void setLookAt(double x, double y, double z, float yMaxRotSpeed, float xMaxRotAngle) {
        wantedX = x;
        wantedY = y;
        wantedZ = z;
        this.yMaxRotSpeed = yMaxRotSpeed;
        this.xMaxRotAngle = xMaxRotAngle;
        lookAtCooldown = 2;
    }

    public void tick() {
        if (resetXRotOnTick()) {
            mob.setXRot(0.0F);
        }
        if (lookAtCooldown > 0) {
            lookAtCooldown--;
            getYRotD().ifPresent(target -> mob.yHeadRot = rotateTowards(mob.yHeadRot, target, yMaxRotSpeed));
            getXRotD().ifPresent(target -> mob.setXRot(rotateTowards(mob.getXRot(), target, xMaxRotAngle)));
        } else {
            mob.yHeadRot = rotateTowards(mob.yHeadRot, mob.yBodyRot, 10.0F);
        }
        clampHeadRotationToBody();
    }

    protected void clampHeadRotationToBody() {
        if (!mob.getNavigation().isDone()) {
            mob.yHeadRot = Mth.rotateIfNecessary(mob.yHeadRot, mob.yBodyRot, mob.getMaxHeadYRot());
        }
    }

    protected boolean resetXRotOnTick() {
        return true;
    }

    public boolean isLookingAtTarget() {
        return lookAtCooldown > 0;
    }

    public double getWantedX() {
        return wantedX;
    }

    public double getWantedY() {
        return wantedY;
    }

    public double getWantedZ() {
        return wantedZ;
    }

    protected Optional<Float> getXRotD() {
        double dx = wantedX - mob.getX();
        double dy = wantedY - mob.getEyeY();
        double dz = wantedZ - mob.getZ();
        double horizontal = Math.sqrt(dx * dx + dz * dz);
        if (Math.abs(dy) <= 1.0E-5 && Math.abs(horizontal) <= 1.0E-5) {
            return Optional.empty();
        }
        return Optional.of((float) -(Mth.atan2(dy, horizontal) * (180.0 / Math.PI)));
    }

    protected Optional<Float> getYRotD() {
        double dx = wantedX - mob.getX();
        double dz = wantedZ - mob.getZ();
        if (Math.abs(dz) <= 1.0E-5 && Math.abs(dx) <= 1.0E-5) {
            return Optional.empty();
        }
        return Optional.of((float) (Mth.atan2(dz, dx) * (180.0 / Math.PI)) - 90.0F);
    }
}
