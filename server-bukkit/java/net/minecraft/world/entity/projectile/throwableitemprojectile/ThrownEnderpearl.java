// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.throwableitemprojectile;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.Items;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
public class ThrownEnderpearl extends ThrowableItemProjectile {
    public ThrownEnderpearl(EntityType<? extends ThrownEnderpearl> type, Level level) {
        super(type, level);
    }
    public ThrownEnderpearl(Level level, LivingEntity mob, ItemStack itemStack) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected Item getDefaultItem() {
        return Items.ENDER_PEARL;
    }
    protected void setOwner(EntityReference<Entity> owner) {
        }
    public Entity getOwner() {
        return super.getOwner();
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void onHit(HitResult hitResult) {
        }
    public void tick() {
    }
    public Entity teleport(TeleportTransition transition) {
        Entity moved = super.teleport(transition);
        if (moved != null) {
            moved.placePortalTicket(BlockPos.containing(moved.position()));
        }
        return moved;
    }
    public boolean canTeleport(Level from, Level to) {
        if (from.dimension() == Level.END && to.dimension() == Level.OVERWORLD && getOwner() instanceof ServerPlayer player) {
            return super.canTeleport(from, to) && player.seenCredits;
        }
        return super.canTeleport(from, to);
    }
    protected void onInsideBlock(BlockState state) {
        }
    public void onRemoval(Entity.RemovalReason reason) {
        super.onRemoval(reason);
    }
    public void onAboveBubbleColumn(boolean dragDown, BlockPos pos) {
        Entity.handleOnAboveBubbleColumn(this, dragDown, pos);
    }
    public void onInsideBubbleColumn(boolean dragDown) {
        Entity.handleOnInsideBubbleColumn(this, dragDown);
    }
}
