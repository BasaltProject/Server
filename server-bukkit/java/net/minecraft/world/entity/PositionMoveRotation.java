// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.PositionAndRotation;
import java.util.Set;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.phys.Vec3;
public record PositionMoveRotation(Vec3 position, Vec3 deltaMovement, float yRot, float xRot) {
    private static double relativeAxis(double current, double change, Set<Relative> relatives, Relative flag) {
        return relatives.contains(flag) ? current + change : change;
    }

    public static final StreamCodec<FriendlyByteBuf, PositionMoveRotation> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.PositionMoveRotation.class, "STREAM_CODEC");
    public static PositionMoveRotation of(Entity entity) {
        PositionAndRotation current = entity.getClientPositionAndRotation();
        return new PositionMoveRotation(current.position(), entity.getKnownMovement(), current.yRot(), current.xRot());
    }
    public PositionMoveRotation withRotation(float yRot, float xRot) {
        return new PositionMoveRotation(position, deltaMovement, yRot, xRot);
    }
    public static PositionMoveRotation of(TeleportTransition transition) {
        return new PositionMoveRotation(transition.position(), transition.deltaMovement(), transition.yRot(), transition.xRot());
    }
    public static PositionMoveRotation calculateAbsolute(PositionMoveRotation source, PositionMoveRotation change, Set<Relative> relatives) {
        Vec3 base = new Vec3(relatives.contains(Relative.X) ? source.position.x : 0.0, relatives.contains(Relative.Y) ? source.position.y : 0.0,
            relatives.contains(Relative.Z) ? source.position.z : 0.0);
        float baseYaw = relatives.contains(Relative.Y_ROT) ? source.yRot : 0.0F;
        float basePitch = relatives.contains(Relative.X_ROT) ? source.xRot : 0.0F;
        float yaw = baseYaw + change.yRot;
        float pitch = net.minecraft.util.Mth.clamp(basePitch + change.xRot, -90.0F, 90.0F);
        Vec3 motion = source.deltaMovement;
        if (relatives.contains(Relative.ROTATE_DELTA)) {
            motion = motion.xRot((float) Math.toRadians(source.xRot - pitch)).yRot((float) Math.toRadians(source.yRot - yaw));
        }
        Vec3 absoluteMotion = new Vec3(relativeAxis(motion.x, change.deltaMovement.x, relatives, Relative.DELTA_X),
            relativeAxis(motion.y, change.deltaMovement.y, relatives, Relative.DELTA_Y),
            relativeAxis(motion.z, change.deltaMovement.z, relatives, Relative.DELTA_Z));
        return new PositionMoveRotation(base.add(change.position), absoluteMotion, yaw, pitch);
    }
}
