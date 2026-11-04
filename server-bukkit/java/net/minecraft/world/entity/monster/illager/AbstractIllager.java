// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.goal.OpenDoorGoal;
import net.minecraft.world.entity.raid.Raider;
import net.minecraft.world.level.Level;
public abstract class AbstractIllager extends Raider {
    protected AbstractIllager(EntityType<? extends AbstractIllager> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    public IllagerArmPose getArmPose() {
        return IllagerArmPose.CROSSED;
    }
    public boolean canAttack(LivingEntity target) {
        return super.canAttack(target);
    }
    protected boolean considersEntityAsAlly(Entity other) {
        return false;
    }
    public static enum IllagerArmPose {
        CROSSED, ATTACKING, SPELLCASTING, BOW_AND_ARROW, CROSSBOW_HOLD, CROSSBOW_CHARGE, CELEBRATING, NEUTRAL;
    }
    protected class RaiderOpenDoorGoal extends OpenDoorGoal {
        public RaiderOpenDoorGoal(Raider raider) {
            super(raider, false);
            this.raider = raider;
        }
        private final Raider raider;
        public boolean canUse() {
            return raider.hasActiveRaid() && super.canUse();
        }
    }
}
