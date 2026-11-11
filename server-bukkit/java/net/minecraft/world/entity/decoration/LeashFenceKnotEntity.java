// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.world.phys.AABB;
import net.minecraft.tags.BlockTags;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.item.Items;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Leashable;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class LeashFenceKnotEntity extends BlockAttachedEntity {
    public static final double OFFSET_Y = 0.375;
    public LeashFenceKnotEntity(EntityType<? extends LeashFenceKnotEntity> type, Level level) {
        super(type, level);
    }
    public LeashFenceKnotEntity(Level level, BlockPos pos) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void recalculateBoundingBox() {
        }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public void dropItem(ServerLevel level, Entity causedBy) {
        playSound(SoundEvents.LEAD_UNTIED, 1.0F, 1.0F);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void notifyLeasheeRemoved(Leashable entity) {
        if (Leashable.leashableLeashedTo(this).isEmpty()) {
            discard();
        }
    }
    public boolean survives() {
        return level().getBlockState(getPos()).is(BlockTags.FENCES);
    }
    public static LeashFenceKnotEntity getOrCreateKnot(Level level, BlockPos pos) {
        return getKnot(level, pos).orElseGet(() -> createKnot(level, pos));
    }
    public static Optional<LeashFenceKnotEntity> getKnot(Level level, BlockPos pos) {
        for (LeashFenceKnotEntity knot : level.getEntitiesOfClass(LeashFenceKnotEntity.class,
                new AABB(pos.getX() - 1.0, pos.getY() - 1.0, pos.getZ() - 1.0, pos.getX() + 1.0, pos.getY() + 1.0, pos.getZ() + 1.0))) {
            if (knot.getPos().equals(pos)) {
                return Optional.of(knot);
            }
        }
        return Optional.empty();
    }
    public static LeashFenceKnotEntity createKnot(Level level, BlockPos pos) {
        LeashFenceKnotEntity knot = new LeashFenceKnotEntity(level, pos);
        level.addFreshEntity(knot);
        return knot;
    }
    public void playPlacementSound() {
        playSound(SoundEvents.LEAD_TIED, 1.0F, 1.0F);
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return new ClientboundAddEntityPacket(this, 0, getPos());
    }
    public Vec3 getRopeHoldPosition(float partialTickTime) {
        return getPosition(partialTickTime).add(0.0, 0.2, 0.0);
    }
    public ItemStack getPickResult() {
        return new ItemStack(Items.LEAD);
    }
}
