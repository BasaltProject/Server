// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.core.Direction;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
public class ShulkerBullet extends Projectile {
    public ShulkerBullet(EntityType<? extends ShulkerBullet> type, Level level) {
        super(type, level);
    }
    public ShulkerBullet(Level level, LivingEntity owner, Entity target, Direction.Axis invalidStartAxis) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void checkDespawn() {
    }
    protected double getDefaultGravity() {
        return live() instanceof org.bukkit.entity.ShulkerBullet api ? (double) api.getDefaultGravity() : 0;
    }
    public void tick() {
    }
    protected boolean isAffectedByBlocks() {
        return false;
    }
    protected boolean canHitEntity(Entity entity) {
        return false;
    }
    public boolean isOnFire() {
        return false;
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public float getLightLevelDependentMagicValue() {
        return 1.0F;
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void onHitBlock(BlockHitResult hitResult) {
        }
    protected void onRedirectProjectile(Projectile hitProjectile) {
        }
    protected void onHit(HitResult hitResult) {
        }
    public boolean isPickable() {
        return super.isPickable();
    }
    public boolean hurtClient(DamageSource source) {
        return true;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
}
