// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile;
import net.minecraft.world.entity.boss.wither.WitherBoss;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public class WitherSkull extends AbstractHurtingProjectile {
    private boolean dangerousFlag;
    public WitherSkull(EntityType<? extends WitherSkull> type, Level level) {
        super(type, level);
    }
    public WitherSkull(Level level, LivingEntity mob, Vec3 direction) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected float getInertia() {
        return 0;
    }
    public boolean isOnFire() {
        return false;
    }
    public float getBlockExplosionResistance(Explosion explosion, BlockGetter level, BlockPos pos, BlockState block, FluidState fluid, float resistance) {
        return isDangerous() && WitherBoss.canDestroy(block) ? Math.min(0.8F, resistance) : resistance;
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void onHit(HitResult hitResult) {
        }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isDangerous() {
        return dangerousFlag;
    }
    public void setDangerous(boolean value) {
        this.dangerousFlag = value;
    }
    protected boolean shouldBurn() {
        return false;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
}
