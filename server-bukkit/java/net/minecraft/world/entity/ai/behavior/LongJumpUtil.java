// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.Pose;
import net.minecraft.util.Mth;
import net.minecraft.util.Mth;
import java.util.Optional;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.phys.Vec3;
public final class LongJumpUtil {
    private static boolean clearBetween(Mob body, EntityDimensions size, Vec3 start, Vec3 end) {
        Vec3 span = end.subtract(start);
        double step = Math.min(size.width(), size.height());
        int checks = Mth.ceil(span.length() / step);
        Vec3 direction = span.normalize();
        Vec3 probe = start;
        for (int at = 0; at < checks; at++) {
            probe = at == checks - 1 ? end : probe.add(direction.scale(step * 0.9));
            if (!body.level().noCollision(body, size.makeBoundingBox(probe))) {
                return false;
            }
        }
        return true;
    }
    public static Optional<Vec3> calculateJumpVectorForAngle(Mob body, Vec3 targetPos, float maxJumpVelocity, int angle, boolean checkCollision) {
        Vec3 from = body.position();
        Vec3 flat = new Vec3(targetPos.x - from.x, 0.0, targetPos.z - from.z).normalize().scale(0.5);
        Vec3 offset = targetPos.subtract(flat).subtract(from);
        float radians = (float) angle * (float) Math.PI / 180.0F;
        double heading = Math.atan2(offset.z, offset.x);
        double reachSqr = offset.subtract(0.0, offset.y, 0.0).lengthSqr();
        double reach = Math.sqrt(reachSqr);
        double gravity = body.getGravity();
        double cosSqr = Math.pow(Math.cos(radians), 2.0);
        double speedSqr = reachSqr * gravity / (reach * Math.sin(2.0F * radians) - 2.0 * offset.y * cosSqr);
        if (speedSqr < 0.0) {
            return Optional.empty();
        }
        double speed = Math.sqrt(speedSqr);
        if (speed > maxJumpVelocity) {
            return Optional.empty();
        }
        double flatSpeed = speed * Math.cos(radians);
        double riseSpeed = speed * Math.sin(radians);
        if (checkCollision) {
            int samples = Mth.ceil(reach / flatSpeed) * 2;
            EntityDimensions size = body.getDimensions(Pose.LONG_JUMPING);
            Vec3 previous = null;
            double travelled = 0.0;
            for (int at = 0; at < samples - 1; at++) {
                travelled += reach / samples;
                double height = Math.sin(radians) / Math.cos(radians) * travelled - travelled * travelled * gravity / (2.0 * speedSqr * cosSqr);
                Vec3 sample = new Vec3(from.x + travelled * Math.cos(heading), from.y + height, from.z + travelled * Math.sin(heading));
                if (previous != null && !clearBetween(body, size, previous, sample)) {
                    return Optional.empty();
                }
                previous = sample;
            }
        }
        return Optional.of(new Vec3(flatSpeed * Math.cos(heading), riseSpeed, flatSpeed * Math.sin(heading)).scale(0.95));
    }
}
