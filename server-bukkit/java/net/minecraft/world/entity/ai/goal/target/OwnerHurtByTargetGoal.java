// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public class OwnerHurtByTargetGoal extends TargetGoal {
    private final TamableAnimal tameAnimal;
    private LivingEntity ownerLastHurtBy;
    private int timestamp;

    public OwnerHurtByTargetGoal(TamableAnimal tameAnimal) {
        super(tameAnimal, false);
        this.tameAnimal = tameAnimal;
        setFlags(EnumSet.of(Goal.Flag.TARGET));
    }

    @Override
    public boolean canUse() {
        if (!tameAnimal.isTame() || tameAnimal.isOrderedToSit()) {
            return false;
        }
        LivingEntity owner = tameAnimal.getOwner();
        if (owner == null) {
            return false;
        }
        ownerLastHurtBy = owner.getLastHurtByMob();
        return owner.getLastHurtByMobTimestamp() != timestamp && canAttack(ownerLastHurtBy, TargetingConditions.DEFAULT)
                && tameAnimal.wantsToAttack(ownerLastHurtBy, owner);
    }

    @Override
    public void start() {
        mob.setTarget(ownerLastHurtBy);
        LivingEntity owner = tameAnimal.getOwner();
        if (owner != null) {
            timestamp = owner.getLastHurtByMobTimestamp();
        }
        super.start();
    }
}
