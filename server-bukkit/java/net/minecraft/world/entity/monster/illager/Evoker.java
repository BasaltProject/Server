// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;
import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.level.Level;
public class Evoker extends SpellcasterIllager {
    public Evoker(EntityType<? extends Evoker> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.monster.Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.5)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 12.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 24.0);
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    protected boolean considersEntityAsAlly(Entity other) {
        return false;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.evoker.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.evoker.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.evoker.hurt");
    }
    protected SoundEvent getCastingSoundEvent() {
        return SoundEvents.byId("entity.evoker.cast_spell");
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        
    }
    class EvokerCastingSpellGoal extends SpellcasterIllager.SpellcasterCastingSpellGoal {
        private EvokerCastingSpellGoal() {
        }
    }
    class EvokerAttackSpellGoal extends SpellcasterIllager.SpellcasterUseSpellGoal {
        private EvokerAttackSpellGoal() {
        }
        protected int getCastingTime() {
            return 40;
        }
        protected int getCastingInterval() {
            return 100;
        }
        protected void performSpellCasting() {
            LivingEntity target = getTarget();
            if (target == null) {
                return;
            }
            double minY = Math.min(target.getY(), getY());
            double maxY = Math.max(target.getY(), getY()) + 1.0;
            float angle = (float) Math.atan2(target.getZ() - getZ(), target.getX() - getX());
            if (distanceToSqr(target) < 9.0) {
                for (int i = 0; i < 5; i++) {
                    float a = angle + i * (float) Math.PI * 0.4F;
                    createFangs(getX() + Math.cos(a) * 1.5, getZ() + Math.sin(a) * 1.5, minY, maxY, a, 0);
                }
                for (int i = 0; i < 8; i++) {
                    float a = angle + i * (float) Math.PI * 2.0F / 8.0F + 1.2566371F;
                    createFangs(getX() + Math.cos(a) * 2.5, getZ() + Math.sin(a) * 2.5, minY, maxY, a, 3);
                }
            } else {
                for (int i = 0; i < 16; i++) {
                    double spacing = 1.25 * (i + 1);
                    createFangs(getX() + Math.cos(angle) * spacing, getZ() + Math.sin(angle) * spacing, minY, maxY, angle, i);
                }
            }
        }
        private void createFangs(double x, double z, double minY, double maxY, float yRot, int warmup) {
            BlockPos pos = BlockPos.containing(x, maxY, z);
            while (pos.getY() >= Math.floor(minY) - 1) {
                if (level().getBlockState(pos.below()).isSolid()) {
                    org.bukkit.entity.Evoker owner = (org.bukkit.entity.Evoker) live();
                    org.bukkit.Location where = new org.bukkit.Location(owner.getWorld(), x, pos.getY(), z, yRot * (180.0F / (float) Math.PI), 0.0F);
                    owner.getWorld().spawn(where, org.bukkit.entity.EvokerFangs.class, fangs -> {
                        fangs.setOwner(owner);
                        fangs.setAttackDelay(warmup);
                    });
                    return;
                }
                pos = pos.below();
            }
        }
        protected SoundEvent getSpellPrepareSound() {
            return SoundEvents.byId("entity.evoker.prepare_attack");
        }
        protected SpellcasterIllager.IllagerSpell getSpell() {
            return SpellcasterIllager.IllagerSpell.FANGS;
        }
    }
    class EvokerSummonSpellGoal extends SpellcasterIllager.SpellcasterUseSpellGoal {
        private EvokerSummonSpellGoal() {
        }
        public boolean canUse() {
            if (!super.canUse()) {
                return false;
            }
            int vexes = level().getEntitiesOfClass(net.minecraft.world.entity.monster.Vex.class, getBoundingBox().inflate(16.0)).size();
            return getRandom().nextInt(8) + 1 > vexes;
        }
        protected int getCastingTime() {
            return 100;
        }
        protected int getCastingInterval() {
            return 340;
        }
        protected void performSpellCasting() {
            org.bukkit.entity.Evoker owner = (org.bukkit.entity.Evoker) live();
            for (int i = 0; i < 3; i++) {
                org.bukkit.Location where = owner.getLocation().add(-2 + getRandom().nextInt(5), 1, -2 + getRandom().nextInt(5));
                int lifetime = 20 * (30 + getRandom().nextInt(90));
                owner.getWorld().spawn(where, org.bukkit.entity.Vex.class, vex -> {
                    vex.setSummoner(owner);
                    vex.setLimitedLifetimeTicks(lifetime);
                    vex.setLimitedLifetime(true);
                });
            }
        }
        protected SoundEvent getSpellPrepareSound() {
            return SoundEvents.byId("entity.evoker.prepare_summon");
        }
        protected SpellcasterIllager.IllagerSpell getSpell() {
            return SpellcasterIllager.IllagerSpell.SUMMON_VEX;
        }
    }
    public class EvokerWololoSpellGoal extends SpellcasterIllager.SpellcasterUseSpellGoal {
        public EvokerWololoSpellGoal() {
            super();
        }
        public boolean canUse() {
            if (getTarget() != null || isCastingSpell() || tickCount < nextAttackTickCount) {
                return false;
            }
            org.bukkit.entity.Evoker owner = (org.bukkit.entity.Evoker) live();
            if (!Boolean.TRUE.equals(owner.getWorld().getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING))) {
                return false;
            }
            java.util.List<org.bukkit.entity.Sheep> blue = owner.getNearbyEntities(16.0, 4.0, 16.0).stream()
                .filter(near -> near instanceof org.bukkit.entity.Sheep sheep && sheep.getColor() == org.bukkit.DyeColor.BLUE)
                .map(near -> (org.bukkit.entity.Sheep) near).toList();
            if (blue.isEmpty()) {
                return false;
            }
            owner.setWololoTarget(blue.get(random.nextInt(blue.size())));
            return true;
        }
        public boolean canContinueToUse() {
            return ((org.bukkit.entity.Evoker) live()).getWololoTarget() != null && attackWarmupDelay > 0;
        }
        public void stop() {
            super.stop();
            ((org.bukkit.entity.Evoker) live()).setWololoTarget(null);
        }
        protected void performSpellCasting() {
            org.bukkit.entity.Sheep target = ((org.bukkit.entity.Evoker) live()).getWololoTarget();
            if (target != null && !target.isDead()) {
                target.setColor(org.bukkit.DyeColor.RED);
            }
        }
        protected int getCastWarmupTime() {
            return 40;
        }
        protected int getCastingTime() {
            return 60;
        }
        protected int getCastingInterval() {
            return 140;
        }
        protected SoundEvent getSpellPrepareSound() {
            return SoundEvents.byId("entity.evoker.prepare_wololo");
        }
        protected SpellcasterIllager.IllagerSpell getSpell() {
            return SpellcasterIllager.IllagerSpell.WOLOLO;
        }
    }
}
