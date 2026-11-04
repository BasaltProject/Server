// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.animal.wolf.Wolf;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.InteractionHand;

public class BegGoal extends Goal {
    private final Wolf wolf;
    private Player player;
    private final float lookDistance;
    private int lookTime;
    private final TargetingConditions begTargeting;

    public BegGoal(Wolf wolf, float lookDistance) {
        this.wolf = wolf;
        this.lookDistance = lookDistance;
        this.begTargeting = TargetingConditions.forNonCombat().range(lookDistance);
        setFlags(EnumSet.of(Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        Player nearest = wolf.level().getNearestPlayer(begTargeting, wolf);
        player = nearest;
        return nearest != null && playerHoldingInteresting(nearest);
    }

    @Override
    public boolean canContinueToUse() {
        if (player == null || !player.isAlive() || wolf.distanceToSqr(player) > (double) (lookDistance * lookDistance)) {
            return false;
        }
        return lookTime > 0 && playerHoldingInteresting(player);
    }

    @Override
    public void start() {
        wolf.setIsInterested(true);
        lookTime = adjustedTickDelay(40 + wolf.getRandom().nextInt(40));
    }

    @Override
    public void stop() {
        wolf.setIsInterested(false);
        player = null;
    }

    @Override
    public void tick() {
        wolf.getLookControl().setLookAt(player.getX(), player.getEyeY(), player.getZ(), 10.0F, wolf.getMaxHeadXRot());
        lookTime--;
    }

    private boolean playerHoldingInteresting(Player candidate) {
        for (InteractionHand hand : InteractionHand.values()) {
            ItemStack held = candidate.getItemInHand(hand);
            if (wolf.isTame() ? wolf.isFood(held) : held.is(net.minecraft.world.item.Items.BONE)) {
                return true;
            }
        }
        return false;
    }
}
