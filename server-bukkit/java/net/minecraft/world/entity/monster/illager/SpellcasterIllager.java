// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;

import java.util.EnumSet;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public abstract class SpellcasterIllager extends AbstractIllager {
    protected int spellCastingTickCount;
    private IllagerSpell currentSpell = IllagerSpell.NONE;

    protected SpellcasterIllager(EntityType<? extends SpellcasterIllager> type, Level level) {
        super(type, level);
    }

    @Override
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }

    @Override
    protected void readAdditionalSaveData(ValueInput input) {
        spellCastingTickCount = input.getIntOr("SpellTicks", 0);
    }

    @Override
    protected void addAdditionalSaveData(ValueOutput output) {
        output.putInt("SpellTicks", spellCastingTickCount);
    }

    @Override
    public AbstractIllager.IllagerArmPose getArmPose() {
        return isCastingSpell() ? AbstractIllager.IllagerArmPose.SPELLCASTING : AbstractIllager.IllagerArmPose.CROSSED;
    }

    public boolean isCastingSpell() {
        return getCurrentSpell() != IllagerSpell.NONE;
    }

    public void setIsCastingSpell(IllagerSpell spell) {
        currentSpell = spell;
        if (live() instanceof org.bukkit.entity.Spellcaster api) {
            api.setSpell(org.bukkit.entity.Spellcaster.Spell.values()[spell.ordinal()]);
        }
    }

    protected IllagerSpell getCurrentSpell() {
        if (live() instanceof org.bukkit.entity.Spellcaster api) {
            return IllagerSpell.values()[api.getSpell().ordinal()];
        }
        return currentSpell;
    }

    @Override
    protected void customServerAiStep(ServerLevel level) {
        super.customServerAiStep(level);
        if (spellCastingTickCount > 0) {
            spellCastingTickCount--;
        }
    }

    protected int getSpellCastingTime() {
        return spellCastingTickCount;
    }

    protected abstract SoundEvent getCastingSoundEvent();

    protected class SpellcasterCastingSpellGoal extends Goal {
        public SpellcasterCastingSpellGoal() {
            setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }

        @Override
        public boolean canUse() {
            return getSpellCastingTime() > 0;
        }

        @Override
        public void start() {
            super.start();
            getNavigation().stop();
        }

        @Override
        public void stop() {
            super.stop();
            setIsCastingSpell(IllagerSpell.NONE);
        }

        @Override
        public void tick() {
            LivingEntity target = getTarget();
            if (target != null) {
                getLookControl().setLookAt(target, getMaxHeadYRot(), getMaxHeadXRot());
            }
        }
    }

    protected abstract class SpellcasterUseSpellGoal extends Goal {
        protected int attackWarmupDelay;
        protected int nextAttackTickCount;

        protected SpellcasterUseSpellGoal() {
        }

        @Override
        public boolean canUse() {
            LivingEntity target = getTarget();
            if (target == null || !target.isAlive() || isCastingSpell()) {
                return false;
            }
            return tickCount >= nextAttackTickCount;
        }

        @Override
        public boolean canContinueToUse() {
            LivingEntity target = getTarget();
            return target != null && target.isAlive() && attackWarmupDelay > 0;
        }

        @Override
        public void start() {
            attackWarmupDelay = adjustedTickDelay(getCastWarmupTime());
            spellCastingTickCount = getCastingTime();
            nextAttackTickCount = tickCount + getCastingInterval();
            SoundEvent prepare = getSpellPrepareSound();
            if (prepare != null) {
                playSound(prepare, 1.0F, 1.0F);
            }
            setIsCastingSpell(getSpell());
        }

        @Override
        public void tick() {
            attackWarmupDelay--;
            if (attackWarmupDelay == 0) {
                performSpellCasting();
                SoundEvent casting = getCastingSoundEvent();
                if (casting != null) {
                    playSound(casting, 1.0F, 1.0F);
                }
            }
        }

        protected abstract void performSpellCasting();

        protected int getCastWarmupTime() {
            return 20;
        }

        protected abstract int getCastingTime();

        protected abstract int getCastingInterval();

        protected abstract SoundEvent getSpellPrepareSound();

        protected abstract IllagerSpell getSpell();
    }

    protected static enum IllagerSpell {
        NONE, SUMMON_VEX, FANGS, WOLOLO, DISAPPEAR, BLINDNESS;

        public static IllagerSpell byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
    }
}
