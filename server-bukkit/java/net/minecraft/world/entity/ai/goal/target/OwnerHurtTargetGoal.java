// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public class OwnerHurtTargetGoal extends TargetGoal {
    private final TamableAnimal tameAnimal;
    private LivingEntity ownerLastHurt;
    private int timestamp;

    public OwnerHurtTargetGoal(TamableAnimal tameAnimal) {
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
        ownerLastHurt = owner.getLastHurtMob();
        return owner.getLastHurtMobTimestamp() != timestamp && canAttack(ownerLastHurt, TargetingConditions.DEFAULT)
                && tameAnimal.wantsToAttack(ownerLastHurt, owner);
    }

    @Override
    public void start() {
        mob.setTarget(ownerLastHurt);
        LivingEntity owner = tameAnimal.getOwner();
        if (owner != null) {
            timestamp = owner.getLastHurtMobTimestamp();
        }
        super.start();
    }
}
