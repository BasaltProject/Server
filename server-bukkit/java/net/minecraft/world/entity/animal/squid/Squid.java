// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.squid;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.animal.AgeableWaterCreature;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.phys.Vec3;
public class Squid extends AgeableWaterCreature {
    public float xBodyRot;
    public float xBodyRotO;
    public float zBodyRot;
    public float zBodyRotO;
    public float tentacleMovement;
    public float oldTentacleMovement;
    public float tentacleAngle;
    public float oldTentacleAngle;
    public Squid(EntityType<? extends Squid> type, Level level) {
        super(type, level);
    }
    float tx;
    float ty;
    float tz;
    public void setMovementVector(float x, float y, float z) {
        this.tx = x;
        this.ty = y;
        this.tz = z;
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.squid.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.squid.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.squid.death");
    }
    protected SoundEvent getSquirtSound() {
        return null;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Squid api ? (float) api.getSoundVolume() : 0;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected double getDefaultGravity() {
        return live() instanceof org.bukkit.entity.Squid api ? (double) api.getDefaultGravity() : 0;
    }
    public void aiStep() {
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected ParticleOptions getInkParticle() {
        return ParticleTypes.SQUID_INK;
    }
    public void travel(Vec3 input) {
        move(net.minecraft.world.entity.MoverType.SELF, getDeltaMovement());
        if (isInWater()) {
            setDeltaMovement(getDeltaMovement().add(tx * 0.05, ty * 0.05, tz * 0.05).scale(0.98));
        } else {
            setDeltaMovement(getDeltaMovement().add(0.0, -0.08, 0.0).scale(0.98));
        }
    }
    public void handleEntityEvent(byte id) {
    }
    public boolean hasMovementVector() {
        return tx != 0.0F || ty != 0.0F || tz != 0.0F;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    static class SquidRandomMovementGoal extends Goal {
        private final Squid squid;
        public SquidRandomMovementGoal(Squid squid) {
            this.squid = squid;
        }
        public boolean canUse() {
            return true;
        }
        public void tick() {
            if (squid.getNoActionTime() > 100) {
                squid.setMovementVector(0.0F, 0.0F, 0.0F);
            } else if (squid.getRandom().nextInt(reducedTickDelay(50)) == 0 || !squid.isInWater() || !squid.hasMovementVector()) {
                float angle = squid.getRandom().nextFloat() * (float) (Math.PI * 2);
                squid.setMovementVector((float) Math.cos(angle) * 0.2F, -0.1F + squid.getRandom().nextFloat() * 0.2F,
                    (float) Math.sin(angle) * 0.2F);
            }
        }
    }
    class SquidFleeGoal extends Goal {
        private int fleeTicks;
        private SquidFleeGoal() {
        }
        public boolean canUse() {
            LivingEntity attacker = getLastHurtByMob();
            return isInWater() && attacker != null && distanceToSqr(attacker) < 100.0;
        }
        public void start() {
            fleeTicks = 0;
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            fleeTicks++;
            LivingEntity attacker = getLastHurtByMob();
            if (attacker == null) {
                return;
            }
            Vec3 away = new Vec3(getX() - attacker.getX(), getY() - attacker.getY(), getZ() - attacker.getZ());
            if (level().getBlockState(BlockPos.containing(getX() + away.x, getY() + away.y, getZ() + away.z)).isAir()) {
                double length = away.length();
                if (length > 0.0) {
                    away = away.scale(1.0 / length);
                    setMovementVector((float) away.x * 3.0F, (float) away.y * 3.0F, (float) away.z * 3.0F);
                }
            }
        }
    }
}
