// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.phys.Vec3;

public abstract class MinecartBehavior {
    protected final AbstractMinecart minecart;

    protected MinecartBehavior(AbstractMinecart minecart) {
        this.minecart = minecart;
    }

    public void onInterpolationStart(InterpolationHandler interpolation) {
    }

    public void lerpMotion(Vec3 movement) {
        minecart.setDeltaMovement(movement);
    }

    public abstract void tick();

    public Level level() {
        return minecart.level();
    }

    public abstract void moveAlongTrack(ServerLevel level);

    public abstract double stepAlongTrack(BlockPos pos, RailShape shape, double movementLeft);

    public abstract boolean pushAndPickupEntities();

    public Vec3 getDeltaMovement() {
        return minecart.getDeltaMovement();
    }

    public void setDeltaMovement(Vec3 deltaMovement) {
        minecart.setDeltaMovement(deltaMovement);
    }

    public void setDeltaMovement(double x, double y, double z) {
        minecart.setDeltaMovement(x, y, z);
    }

    public Vec3 position() {
        return minecart.position();
    }

    public double getX() {
        return minecart.getX();
    }

    public double getY() {
        return minecart.getY();
    }

    public double getZ() {
        return minecart.getZ();
    }

    public void setPos(Vec3 pos) {
        minecart.setPos(pos.x, pos.y, pos.z);
    }

    public void setPos(double x, double y, double z) {
        minecart.setPos(x, y, z);
    }

    public float getXRot() {
        return minecart.getXRot();
    }

    public void setXRot(float rot) {
        minecart.setXRot(rot);
    }

    public float getYRot() {
        return minecart.getYRot();
    }

    public void setYRot(float rot) {
        minecart.setYRot(rot);
    }

    public Direction getMotionDirection() {
        return minecart.getDirection().getClockWise();
    }

    public Vec3 getKnownMovement(Vec3 knownMovement) {
        return knownMovement;
    }

    public abstract double getMaxSpeed(ServerLevel level);

    public abstract double getSlowdownFactor();
}
