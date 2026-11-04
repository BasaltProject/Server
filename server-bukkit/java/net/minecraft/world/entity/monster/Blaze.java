// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.level.Level;
public class Blaze extends Monster {
    public Blaze(EntityType<? extends Blaze> blaze, Level level) {
        super(blaze, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.MoveTowardsRestrictionGoal(this, 1.0));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0, 0.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
        targetSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.player.Player.class, true));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 6.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.23)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 48.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.blaze.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.blaze.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.blaze.death");
    }
    public void aiStep() {
    }
    public boolean isSensitiveToWater() {
        return super.isSensitiveToWater();
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    static class BlazeAttackGoal extends Goal {
        private final Blaze blaze;
        private int attackStep;
        private int attackTime;
        private int lastSeen;
        public BlazeAttackGoal(Blaze blaze) {
            this.blaze = blaze;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            LivingEntity target = blaze.getTarget();
            return target != null && target.isAlive() && blaze.canAttack(target);
        }
        public void start() {
            attackStep = 0;
        }
        public void stop() {
            lastSeen = 0;
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            attackTime--;
            LivingEntity target = blaze.getTarget();
            if (target == null) {
                return;
            }
            boolean visible = blaze.hasLineOfSight(target);
            lastSeen = visible ? 0 : lastSeen + 1;
            double distance = blaze.distanceToSqr(target);
            if (distance < 4.0) {
                if (!visible) {
                    return;
                }
                if (attackTime <= 0) {
                    attackTime = 20;
                    blaze.doHurtTarget(null, target);
                }
                blaze.getMoveControl().setWantedPosition(target.getX(), target.getY(), target.getZ(), 1.0);
            } else if (distance < getFollowDistance() * getFollowDistance() && visible) {
                if (attackTime <= 0) {
                    attackStep++;
                    if (attackStep == 1) {
                        attackTime = 60;
                    } else if (attackStep <= 4) {
                        attackTime = 6;
                    } else {
                        attackTime = 100;
                        attackStep = 0;
                    }
                    if (attackStep > 1) {
                        double spread = Math.sqrt(Math.sqrt(distance)) * 0.5;
                        org.bukkit.entity.Blaze api = (org.bukkit.entity.Blaze) blaze.live();
                        for (int i = 0; i < 1; i++) {
                            org.bukkit.Location where = api.getEyeLocation();
                            org.bukkit.util.Vector aim = new org.bukkit.util.Vector(
                                target.getX() - blaze.getX() + blaze.getRandom().nextGaussian() * spread,
                                target.getY(0.5) - blaze.getY(0.5),
                                target.getZ() - blaze.getZ() + blaze.getRandom().nextGaussian() * spread);
                            api.getWorld().spawn(where, org.bukkit.entity.SmallFireball.class, fireball -> {
                                fireball.setShooter(api);
                                fireball.setDirection(aim);
                            });
                        }
                    }
                }
                blaze.getLookControl().setLookAt(target, 10.0F, 10.0F);
            } else if (lastSeen < 5) {
                blaze.getMoveControl().setWantedPosition(target.getX(), target.getY(), target.getZ(), 1.0);
            }
        }
        private double getFollowDistance() {
            return blaze.getAttributeValue(Attributes.FOLLOW_RANGE);
        }
    }
}
