// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.LinearInterpolationHandler;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
public class FishingHook extends Projectile {
    private Entity hookedInEntity;
    private FishingHook(EntityType<? extends FishingHook> type, Level level, int luck, int lureSpeed) {
        super(type, level);
    }
    public FishingHook(EntityType<? extends FishingHook> type, Level level) {
        super(type, level);
    }
    public FishingHook(Player player, Level level, int luck, int lureSpeed) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected InterpolationHandler createInterpolationHandler() {
        return LinearInterpolationHandler.create(this);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected boolean shouldBounceOnWorldBorder() {
        return false;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public void tick() {
    }
    protected boolean canHitEntity(Entity entity) {
        return false;
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void onHitBlock(BlockHitResult hitResult) {
        }
    public boolean isOpenWaterFishing() {
        return live() instanceof org.bukkit.entity.FishHook api && api.isInOpenWater();
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public int retrieve(ItemStack rod) {
        Player owner = getPlayerOwner();
        if (level().isClientSide() || owner == null) {
            return 0;
        }
        int damage = hookedInEntity == null ? 0 : (hookedInEntity instanceof ItemEntity ? 3 : 5);
        if (onGround()) {
            damage = 2;
        }
        discard();
        return damage;
    }
    public void handleEntityEvent(byte id) {
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    protected void pullEntity(Entity entity) {
        }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    public void remove(Entity.RemovalReason reason) {
        super.remove(reason);
    }
    public void onClientRemoval() {
        
    }
    public void setOwner(Entity owner) {
        super.setOwner(owner);
    }
    public Player getPlayerOwner() {
        return getOwner() instanceof Player player ? player : null;
    }
    public Entity getHookedIn() {
        return hookedInEntity;
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return false;
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        Entity owner = getOwner();
        return new ClientboundAddEntityPacket(this, serverEntity, owner == null ? getId() : owner.getId());
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    static enum FishHookState {
        FLYING, HOOKED_IN_ENTITY, BOBBING;
    }
    static enum OpenWaterType {
        ABOVE_WATER, INSIDE_WATER, INVALID;
    }
}
