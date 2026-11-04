// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.phys.Vec3;
public class Illusioner extends SpellcasterIllager implements RangedAttackMob {
    public static final int ILLUSION_SPREAD = 3;
    public Illusioner(EntityType<? extends Illusioner> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void aiStep() {
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    public Vec3[] getIllusionOffsets(float a) {
        return new Vec3[] {Vec3.ZERO, Vec3.ZERO, Vec3.ZERO, Vec3.ZERO};
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.illusioner.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.illusioner.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.illusioner.hurt");
    }
    protected SoundEvent getCastingSoundEvent() {
        return net.minecraft.sounds.SoundEvents.byId("entity.illusioner.cast_spell");
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    class IllusionerMirrorSpellGoal extends SpellcasterIllager.SpellcasterUseSpellGoal {
        private IllusionerMirrorSpellGoal() {
            super();
        }
        public boolean canUse() {
            return false;
        }
        protected int getCastingTime() {
            return 0;
        }
        protected int getCastingInterval() {
            return 0;
        }
        protected void performSpellCasting() {
                }
        protected SoundEvent getSpellPrepareSound() {
            return null;
        }
        protected SpellcasterIllager.IllagerSpell getSpell() {
        return SpellcasterIllager.IllagerSpell.DISAPPEAR;
    }
    }
    class IllusionerBlindnessSpellGoal extends SpellcasterIllager.SpellcasterUseSpellGoal {
        private IllusionerBlindnessSpellGoal() {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void start() {
            
        }
        protected int getCastingTime() {
            return 0;
        }
        protected int getCastingInterval() {
            return 0;
        }
        protected void performSpellCasting() {
                }
        protected SoundEvent getSpellPrepareSound() {
            return null;
        }
        protected SpellcasterIllager.IllagerSpell getSpell() {
            return SpellcasterIllager.IllagerSpell.BLINDNESS;
        }
    }
}
