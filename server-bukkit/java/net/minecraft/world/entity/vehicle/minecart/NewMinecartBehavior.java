// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class NewMinecartBehavior extends MinecartBehavior {
    public static final int POS_ROT_LERP_TICKS = 3;
    public static final double ON_RAIL_Y_OFFSET = 0.1;
    public static final double OPPOSING_SLOPES_REST_AT_SPEED_THRESHOLD = 0.005;
    public final List<MinecartStep> lerpSteps = null;
    public final List<MinecartStep> currentLerpSteps = null;
    public double currentLerpStepsTotalWeight;
    public MinecartStep oldLerp;
    public NewMinecartBehavior(AbstractMinecart minecart) {
        super(minecart);
    }
    public void tick() {
        return;
    }
    public void setOldLerpValues() {

    }
    public boolean cartHasPosRotLerp() {
        return false;
    }
    public float getCartLerpXRot(float partialTicks) {
        return minecart.getXRot();
    }
    public float getCartLerpYRot(float partialTicks) {
        return minecart.getYRot();
    }
    public Vec3 getCartLerpPosition(float partialTicks) {
        return minecart.position();
    }
    public Vec3 getCartLerpMovements(float partialTicks) {
        return minecart.getDeltaMovement();
    }
    public void adjustToRails(BlockPos targetBlockPos, BlockState currentState, boolean instant) {

    }
    public void moveAlongTrack(ServerLevel level) {
        return;
    }
    public double stepAlongTrack(BlockPos pos, RailShape shape, double movementLeft) {
        return 0.0;
    }
    public double getMaxSpeed(ServerLevel level) {
        return (minecart.isInWater() ? 4.0 : 8.0) / 20.0;
    }
    public double getSlowdownFactor() {
        return minecart.isVehicle() ? 0.997 : 0.975;
    }
    public boolean pushAndPickupEntities() {
        return false;
    }
    public boolean pickupEntities(AABB hitbox) {
        return false;
    }
    public boolean pushEntities(AABB hitbox) {
        return false;
    }
    public static record MinecartStep(Vec3 position, Vec3 movement, float yRot, float xRot, float weight) {
        public static final StreamCodec<ByteBuf, MinecartStep> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.vehicle.minecart.NewMinecartBehavior.MinecartStep.class, "STREAM_CODEC");
        public static final MinecartStep ZERO = null;
    }
    static class TrackIteration {
        public double movementLeft;
        public boolean firstIteration;
        public boolean hasGainedSlopeSpeed;
        public boolean hasHalted;
        public boolean hasBoosted;
        public boolean shouldIterate() {
            return false;
        }
    }
    static record StepPartialTicks(float partialTicksInStep, MinecartStep currentStep, MinecartStep previousStep) {
    }
}
