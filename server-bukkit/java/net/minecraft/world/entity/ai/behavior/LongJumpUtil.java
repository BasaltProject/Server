// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Optional;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.phys.Vec3;

public final class LongJumpUtil {
    public static Optional<Vec3> calculateJumpVectorForAngle(Mob body, Vec3 targetPos, float maxJumpVelocity, int angle, boolean checkCollision) {
        Vec3 from = body.position();
        Vec3 flat = new Vec3(targetPos.x - from.x, 0.0, targetPos.z - from.z).normalize().scale(0.5);
        Vec3 delta = targetPos.subtract(flat).subtract(from);
        float angRad = angle * (float) Math.PI / 180.0F;
        double heading = Math.atan2(delta.z, delta.x);
        double r2 = delta.subtract(0.0, delta.y, 0.0).lengthSqr();
        double r = Math.sqrt(r2);
        double dy = delta.y;
        double g = body.getGravity();
        double sin2 = Math.sin(2.0F * angRad);
        double cosSqr = Math.pow(Math.cos(angRad), 2.0);
        double sinA = Math.sin(angRad);
        double cosA = Math.cos(angRad);
        double sinH = Math.sin(heading);
        double cosH = Math.cos(heading);
        double v0Sqr = r2 * g / (r * sin2 - 2.0 * dy * cosSqr);
        if (v0Sqr < 0.0) {
            return Optional.empty();
        }
        double v0 = Math.sqrt(v0Sqr);
        if (v0 > maxJumpVelocity) {
            return Optional.empty();
        }
        double v0r = v0 * cosA;
        double v0y = v0 * sinA;
        if (checkCollision) {
            int samples = Mth.ceil(r / v0r) * 2;
            double ri = 0.0;
            Vec3 previous = null;
            EntityDimensions dimensions = body.getDimensions(Pose.LONG_JUMPING);
            for (int i = 0; i < samples - 1; i++) {
                ri += r / samples;
                double yi = sinA / cosA * ri - Math.pow(ri, 2.0) * g / (2.0 * v0Sqr * Math.pow(cosA, 2.0));
                Vec3 sample = new Vec3(from.x + ri * cosH, from.y + yi, from.z + ri * sinH);
                if (previous != null && !isClearTransition(body, dimensions, previous, sample)) {
                    return Optional.empty();
                }
                previous = sample;
            }
        }
        return Optional.of(new Vec3(v0r * cosH, v0y, v0r * sinH).scale(0.95));
    }

    private static boolean isClearTransition(Mob body, EntityDimensions dimensions, Vec3 a, Vec3 b) {
        Vec3 direction = b.subtract(a);
        double smallest = Math.min(dimensions.width(), dimensions.height());
        int checks = Mth.ceil(direction.length() / smallest);
        Vec3 step = direction.normalize();
        Vec3 next = a;
        for (int i = 0; i < checks; i++) {
            next = i == checks - 1 ? b : next.add(step.scale(smallest * 0.9));
            if (!body.level().noCollision(body, dimensions.makeBoundingBox(next))) {
                return false;
            }
        }
        return true;
    }
}
