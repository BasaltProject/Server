// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.boat;
import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.BlockUtil;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.entity.Leashable;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.vehicle.VehicleEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractBoat extends VehicleEntity implements Leashable {
    public static final int PADDLE_LEFT = 0;
    public static final int PADDLE_RIGHT = 1;
    public static final double PADDLE_SOUND_TIME = 0;
    public static final int BUBBLE_TIME = 60;
    private final Supplier<Item> dropItem;
    public AbstractBoat(EntityType<? extends AbstractBoat> type, Level level, Supplier<Item> dropItem) {
        super(type, level);
        this.dropItem = dropItem;
    }
    public void setInitialPos(double x, double y, double z) {
        setPos(x, y, z);
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean canCollideWith(Entity entity) {
        return canVehicleCollide(this, entity);
    }
    public static boolean canVehicleCollide(Entity vehicle, Entity entity) {
        return entity.canBeCollidedWith(vehicle) && !vehicle.isPassengerOfSameVehicle(entity);
    }
    public boolean canBeCollidedWith(Entity other) {
        return false;
    }
    public boolean isPushable() {
        return true;
    }
    protected abstract double rideHeight(EntityDimensions dimensions);
    protected Vec3 getPassengerAttachmentPoint(Entity passenger, EntityDimensions dimensions, float scale) {
        float offset = getSinglePassengerXOffset();
        if (getPassengers().size() > 1) {
            offset = getPassengers().indexOf(passenger) == 0 ? 0.2F : -0.6F;
            if (passenger instanceof net.minecraft.world.entity.animal.Animal) {
                offset += 0.2F;
            }
        }
        return new Vec3(0.0, rideHeight(dimensions), offset).yRot(-getYRot() * (float) (Math.PI / 180.0));
    }
    public void onAboveBubbleColumn(boolean dragDown, BlockPos pos) {
        bubbleTime = BUBBLE_TIME;
    }
    public void push(Entity entity) {
        
    }
    public void animateHurt(float yaw) {
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    public Direction getMotionDirection() {
        return getDirection();
    }
    public void tick() {
    }
    public void handleEntityEvent(byte id) {
    }
    protected void handleBubbleColumnEffect(boolean dragDown) {
        }
    protected SoundEvent getPaddleSound() {
        return null;
    }
    private final boolean[] paddleStates = new boolean[2];
    private final float[] paddlePositions = new float[2];
    private float bubbleAngle;
    private float bubbleAngleO;
    private int bubbleTime;
    public void setPaddleState(boolean left, boolean right) {
        paddleStates[0] = left;
        paddleStates[1] = right;
        for (int side = 0; side < 2; side++) {
            paddlePositions[side] = paddleStates[side] ? paddlePositions[side] + (float) (Math.PI / 8.0) : 0.0F;
        }
    }
    public float getRowingTime(int side, float a) {
        return getPaddleState(side)
            ? net.minecraft.util.Mth.clampedLerp(a, paddlePositions[side] - (float) (Math.PI / 8.0), paddlePositions[side]) : 0.0F;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, getBbHeight() * 0.5, 0.0);
    }
    public boolean supportQuadLeash() {
        return false;
    }
    public Vec3[] getQuadLeashOffsets() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.vehicle.boat.AbstractBoat#getQuadLeashOffsets");
    }
    public float getWaterLevelAbove() {
        net.minecraft.world.phys.AABB box = getBoundingBox();
        int minY = net.minecraft.util.Mth.floor(box.maxY);
        int maxY = net.minecraft.util.Mth.ceil(box.maxY);
        for (int y = minY; y < maxY; y++) {
            boolean full = true;
            for (int x = net.minecraft.util.Mth.floor(box.minX); x < net.minecraft.util.Mth.ceil(box.maxX); x++) {
                for (int z = net.minecraft.util.Mth.floor(box.minZ); z < net.minecraft.util.Mth.ceil(box.maxZ); z++) {
                    full &= level().getBlockState(new BlockPos(x, y, z)).getBlock() == net.minecraft.world.level.block.Blocks.WATER;
                }
            }
            if (!full) {
                return y;
            }
        }
        return maxY + 1;
    }
    public float getGroundFriction() {
        net.minecraft.world.phys.AABB box = getBoundingBox();
        float friction = 0.0F;
        int count = 0;
        int y = net.minecraft.util.Mth.floor(box.minY - 0.001) ;
        for (int x = net.minecraft.util.Mth.floor(box.minX); x < net.minecraft.util.Mth.ceil(box.maxX); x++) {
            for (int z = net.minecraft.util.Mth.floor(box.minZ); z < net.minecraft.util.Mth.ceil(box.maxZ); z++) {
                var state = level().getBlockState(new BlockPos(x, y, z));
                if (!state.isAir()) {
                    friction += state.getBlock().getFriction();
                    count++;
                }
            }
        }
        return count == 0 ? 0.0F : friction / count;
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    protected float getAirDrag() {
        return 0;
    }
    protected float getSinglePassengerXOffset() {
        return 0;
    }
    public boolean hasEnoughSpaceFor(Entity entity) {
        return entity.getBbWidth() < getBbWidth();
    }
    protected void positionRider(Entity passenger, Entity.MoveFunction moveFunction) {
    }
    public Vec3 getDismountLocationForPassenger(LivingEntity passenger) {
        Vec3 direction = getCollisionHorizontalEscapeVector(getBbWidth() * net.minecraft.util.Mth.SQRT_OF_TWO,
            passenger.getBbWidth(), passenger.getYRot());
        return new Vec3(getX() + direction.x, getBoundingBox().maxY, getZ() + direction.z);
    }
    protected float clampRotation(Entity passenger) {
        return 0;
    }
    public void onPassengerTurned(Entity passenger) {
        
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean getPaddleState(int side) {
        return side >= 0 && side < 2 && paddleStates[side];
    }
    public float getBubbleAngle(float a) {
        return net.minecraft.util.Mth.lerp(a, bubbleAngleO, bubbleAngle);
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    protected int getMaxPassengers() {
        return 0;
    }
    public LivingEntity getControllingPassenger() {
        return getFirstPassenger() instanceof LivingEntity passenger ? passenger : null;
    }
    public void setInput(boolean left, boolean right, boolean up, boolean down) {
        
    }
    public boolean isUnderWater() {
        return false;
    }
    protected final Item getDropItem() {
        return dropItem.get();
    }
    public final ItemStack getPickResult() {
        return new ItemStack(getDropItem());
    }
    protected AABB modifyPassengerFluidInteractionBox(AABB passengerBox) {
        return passengerBox;
    }
    public static enum Status {
        IN_WATER, UNDER_WATER, UNDER_FLOWING_WATER, ON_LAND, IN_AIR;
    }
    private Leashable.LeashData leashData;
    public Leashable.LeashData getLeashData() {
        return leashData;
    }
    public void setLeashData(Leashable.LeashData leashData) {
        this.leashData = leashData;
    }
}
