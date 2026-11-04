// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.phys.Vec3;
public class OldMinecartBehavior extends MinecartBehavior {
    public OldMinecartBehavior(AbstractMinecart minecart) {
        super(minecart);
    }
    public void onInterpolationStart(InterpolationHandler interpolation) {
    }
    public void lerpMotion(Vec3 movement) {
        setDeltaMovement(movement);
    }
    public void tick() {
        return;
    }
    public void moveAlongTrack(ServerLevel level) {
        return;
    }
    public Vec3 getPosOffs(double x, double y, double z, double offs) {
        return new Vec3(x, y, z);
    }
    public Vec3 getPos(double x, double y, double z) {
        return new Vec3(x, y, z);
    }
    public double stepAlongTrack(BlockPos pos, RailShape shape, double movementLeft) {
        return 0.0;
    }
    public boolean pushAndPickupEntities() {
        return false;
    }
    public Direction getMotionDirection() {
        return minecart.getDirection().getClockWise();
    }
    public Vec3 getKnownMovement(Vec3 knownMovement) {
        return knownMovement;
    }
    public double getMaxSpeed(ServerLevel level) {
        return (minecart.isInWater() ? 4.0 : 8.0) / 20.0;
    }
    public double getSlowdownFactor() {
        return minecart.isVehicle() ? 0.997 : 0.96;
    }
}
