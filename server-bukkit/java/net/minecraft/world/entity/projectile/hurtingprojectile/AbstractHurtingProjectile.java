// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractHurtingProjectile extends Projectile {
    public static final double INITAL_ACCELERATION_POWER = 0.1;
    public static final double DEFLECTION_SCALE = 0.5;
    public double accelerationPower;
    protected AbstractHurtingProjectile(EntityType<? extends AbstractHurtingProjectile> type, Level level) {
        super(type, level);
    }
    protected AbstractHurtingProjectile(EntityType<? extends AbstractHurtingProjectile> type, double x, double y, double z, Level level) {
        super(type, level);
    }
    public AbstractHurtingProjectile(EntityType<? extends AbstractHurtingProjectile> type, double x, double y, double z, Vec3 direction, Level level) {
        super(type, level);
    }
    public AbstractHurtingProjectile(EntityType<? extends AbstractHurtingProjectile> type, LivingEntity mob, Vec3 direction, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    protected ClipContext.Block getClipType() {
        return net.minecraft.world.level.ClipContext.Block.COLLIDER;
    }
    public void tick() {
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected boolean canHitEntity(Entity entity) {
        return false;
    }
    protected boolean shouldBurn() {
        return false;
    }
    protected ParticleOptions getTrailParticle() {
        return net.minecraft.core.particles.ParticleTypes.SMOKE;
    }
    protected float getInertia() {
        return 0;
    }
    protected float getLiquidInertia() {
        return 0;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public float getLightLevelDependentMagicValue() {
        return 1.0F;
    }
    protected void onDeflection(boolean byAttack) {
        }
}
