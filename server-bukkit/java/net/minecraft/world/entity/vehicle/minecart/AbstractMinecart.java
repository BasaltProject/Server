// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import com.mojang.datafixers.util.Pair;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.BlockUtil;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.vehicle.VehicleEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractMinecart extends VehicleEntity {
    private Optional<BlockState> customDisplayBlockState = Optional.empty();
    protected static final float WATER_SLOWDOWN_FACTOR = 0.95F;
    protected AbstractMinecart(EntityType<?> type, Level level) {
        super(type, level);
    }
    protected AbstractMinecart(EntityType<?> type, Level level, double x, double y, double z) {
        super(type, level);
    }
    public void setInitialPos(double x, double y, double z) {
        setPos(x, y, z);
    }
    public static <T extends AbstractMinecart> T createMinecart(Level level, double x, double y, double z, EntityType<T> type, EntitySpawnReason reason, ItemStack itemStack, Player player) {
        T cart = (T) type.create(level, reason);
        if (cart != null) {
            cart.snapTo(x, y, z);
        }
        return cart;
    }
    private MinecartBehavior behavior;
    public MinecartBehavior getBehavior() {
        if (behavior == null) {
            behavior = new OldMinecartBehavior(this);
        }
        return behavior;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean canCollideWith(Entity entity) {
        return net.minecraft.world.entity.vehicle.boat.AbstractBoat.canVehicleCollide(this, entity);
    }
    public boolean isPushable() {
        return true;
    }
    protected Vec3 getPassengerAttachmentPoint(Entity passenger, EntityDimensions dimensions, float scale) {
        return new Vec3(0.0, dimensions.height() * 0.5, 0.0);
    }
    protected float getBlockSpeedFactor() {
        return super.getBlockSpeedFactor();
    }
    public void animateHurt(float yaw) {
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    public static Pair<Vec3i, Vec3i> exits(RailShape shape) {
        return switch (shape) {
            case NORTH_SOUTH -> Pair.of(new Vec3i(0, 0, -1), new Vec3i(0, 0, 1));
            case EAST_WEST -> Pair.of(new Vec3i(-1, 0, 0), new Vec3i(1, 0, 0));
            case ASCENDING_EAST -> Pair.of(new Vec3i(-1, 0, 0), new Vec3i(1, 1, 0));
            case ASCENDING_WEST -> Pair.of(new Vec3i(-1, 1, 0), new Vec3i(1, 0, 0));
            case ASCENDING_NORTH -> Pair.of(new Vec3i(0, 1, -1), new Vec3i(0, 0, 1));
            case ASCENDING_SOUTH -> Pair.of(new Vec3i(0, 0, -1), new Vec3i(0, 1, 1));
            case SOUTH_EAST -> Pair.of(new Vec3i(0, 0, 1), new Vec3i(1, 0, 0));
            case SOUTH_WEST -> Pair.of(new Vec3i(0, 0, 1), new Vec3i(-1, 0, 0));
            case NORTH_WEST -> Pair.of(new Vec3i(0, 0, -1), new Vec3i(-1, 0, 0));
            case NORTH_EAST -> Pair.of(new Vec3i(0, 0, -1), new Vec3i(1, 0, 0));
        };
    }
    public Direction getMotionDirection() {
        return getDirection();
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    public void tick() {
    }
    public boolean isFirstTick() {
        return false;
    }
    public BlockPos getCurrentBlockPosOrRailBelow() {
        BlockPos pos = blockPosition();
        BlockPos below = pos.below();
        return !(level().getBlockState(pos).getBlock() instanceof net.minecraft.world.level.block.BaseRailBlock)
            && level().getBlockState(below).getBlock() instanceof net.minecraft.world.level.block.BaseRailBlock ? below : pos;
    }
    protected double getMaxSpeed(ServerLevel level) {
        return 0;
    }
    public void activateMinecart(ServerLevel level, int xt, int yt, int zt, boolean state) {

    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    protected void moveAlongTrack(ServerLevel level) {
        }
    protected void comeOffTrack(ServerLevel level) {
        }
    protected float getAirDrag() {
        return 0;
    }
    protected double makeStepAlongTrack(BlockPos pos, RailShape shape, double movementLeft) {
        return 0;
    }
    public void applyEffectsFromBlocks() {

    }
    public boolean isOnRails() {
        return false;
    }
    public void setOnRails(boolean onRails) {
        
    }
    public boolean isFlipped() {
        return false;
    }
    public void setFlipped(boolean flipped) {
        
    }
    public Vec3 getRedstoneDirection(BlockPos pos) {
        return Vec3.ZERO;
    }
    public boolean isRedstoneConductor(BlockPos pos) {
        return level().getBlockState(pos).isRedstoneConductor(level(), pos);
    }
    protected Vec3 applyNaturalSlowdown(Vec3 movement) {
        double slowdown = isVehicle() ? 0.997 : 0.96;
        Vec3 result = movement.multiply(slowdown, 0.0, slowdown);
        return isInWater() ? result.scale(0.95F) : result;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void push(Entity entity) {
        
    }
    public BlockState getDisplayBlockState() {
        return customDisplayBlockState.orElseGet(this::getDefaultDisplayBlockState);
    }
    public BlockState getDefaultDisplayBlockState() {
        return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
    }
    public int getDisplayOffset() {
        return getDefaultDisplayOffset();
    }
    public int getDefaultDisplayOffset() {
        return 6;
    }
    public void setCustomDisplayBlockState(Optional<BlockState> state) {
        customDisplayBlockState = state;
    }
    public void setDisplayOffset(int offset) {
        
    }
    public static boolean useExperimentalMovement(Level level) {
        return false;
    }
    public abstract ItemStack getPickResult();
    public boolean isRideable() {
        return false;
    }
    public boolean isFurnace() {
        return false;
    }
}
