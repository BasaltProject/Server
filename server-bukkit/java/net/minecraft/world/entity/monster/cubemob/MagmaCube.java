// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.cubemob;
import net.minecraft.core.BlockPos;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.monster.Enemy;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.material.Fluid;
public class MagmaCube extends AbstractCubeMob implements Enemy {
    public MagmaCube(EntityType<? extends MagmaCube> type, Level level) {
        super(type, level);
    }
    protected void addBehaviourGoals() {
        }
    protected void addTargetingGoals() {
        }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public static boolean checkMagmaCubeSpawnRules(EntityType<MagmaCube> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public void setSize(int size, boolean updateHealth) {
        if (live() instanceof org.bukkit.entity.Slime api) {
            api.setSize(size);
        }
    }
    protected ParticleOptions getParticleType() {
        return net.minecraft.core.particles.ParticleTypes.FLAME;
    }
    protected int getJumpDelay() {
        return 0;
    }
    protected void decreaseSquish() {
        }
    protected void jumpInLiquid(TagKey<Fluid> type) {
        }
    protected boolean canDealDamage() {
        return false;
    }
    protected float getAttackDamage() {
        return 0;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.magma_cube.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.magma_cube.death");
    }
    protected SoundEvent getSquishSound() {
        return null;
    }
    protected SoundEvent getJumpSound() {
        return null;
    }
    protected boolean canBeABaby() {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
}
