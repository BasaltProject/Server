// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.cubemob;
import net.minecraft.core.BlockPos;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.monster.Enemy;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
public class Slime extends AbstractCubeMob implements Enemy {
    public Slime(EntityType<? extends Slime> type, Level level) {
        super(type, level);
    }
    protected void addBehaviourGoals() {
        }
    protected void addTargetingGoals() {
        }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.slime.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.slime.death");
    }
    protected ParticleOptions getParticleType() {
        return net.minecraft.core.particles.ParticleTypes.ITEM_SLIME;
    }
    protected SoundEvent getSquishSound() {
        return null;
    }
    protected SoundEvent getJumpSound() {
        return null;
    }
    public static boolean checkSlimeSpawnRules(EntityType<Slime> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected boolean canBeABaby() {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void setSize(int size, boolean updateHealth) {
        if (live() instanceof org.bukkit.entity.Slime api) {
            api.setSize(size);
        }
    }
}
