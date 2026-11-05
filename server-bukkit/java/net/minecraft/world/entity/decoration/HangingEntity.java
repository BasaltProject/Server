// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.phys.AABB;
public abstract class HangingEntity extends BlockAttachedEntity {
    protected HangingEntity(EntityType<? extends HangingEntity> type, Level level) {
        super(type, level);
    }
    protected HangingEntity(EntityType<? extends HangingEntity> type, Level level, BlockPos pos) {
        super(type, level, pos);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public Direction getDirection() {
        return live() instanceof org.bukkit.entity.Hanging h ? Direction.valueOf(h.getFacing().name()) : Direction.SOUTH;
    }
    protected void setDirectionRaw(Direction direction) {
        }
    protected void setDirection(Direction direction) {
        }
    protected void recalculateBoundingBox() {
        }
    protected abstract AABB calculateBoundingBox(BlockPos pos, Direction direction);
    public boolean survives() {
        return true;
    }
    protected AABB calculateSupportBox() {
        return getBoundingBox();
    }
    protected boolean canCoexist(boolean allowIntersectingSameType) {
        return false;
    }
    protected boolean hasLevelCollision(AABB popBox) {
        return false;
    }
    protected AABB getPopBox() {
        return getBoundingBox();
    }
    public abstract void playPlacementSound();
    public ItemEntity spawnAtLocation(ServerLevel level, ItemStack itemStack, float yOffs) {
        ItemEntity item = new ItemEntity(level(), getX() + getDirection().getStepX() * 0.15F, getY() + yOffs,
            getZ() + getDirection().getStepZ() * 0.15F, itemStack);
        item.setDefaultPickUpDelay();
        level().addFreshEntity(item);
        return item;
    }
    public float rotate(Rotation rotation) {
        return getYRot();
    }
    public float mirror(Mirror mirror) {
        return getYRot();
    }
}
