// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.golem.IronGolem;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.AABB;

public class DefendVillageTargetGoal extends TargetGoal {
    private final IronGolem golem;
    private LivingEntity potentialTarget;
    private final TargetingConditions attackTargeting = TargetingConditions.forCombat().range(64.0);

    public DefendVillageTargetGoal(IronGolem golem) {
        super(golem, false, true);
        this.golem = golem;
        setFlags(EnumSet.of(Goal.Flag.TARGET));
    }

    @Override
    public boolean canUse() {
        AABB area = golem.getBoundingBox().inflate(10.0, 8.0, 10.0);
        potentialTarget = null;
        for (Villager villager : golem.level().getEntitiesOfClass(Villager.class, area)) {
            for (Player player : golem.level().getEntitiesOfClass(Player.class, area, p -> attackTargeting.test(getServerLevel(golem), golem, p))) {
                if (villager.getPlayerReputation(player) <= -100) {
                    potentialTarget = player;
                }
            }
        }
        if (potentialTarget == null) {
            return false;
        }
        return !(potentialTarget instanceof Player player) || !player.isSpectator() && !player.isCreative();
    }

    @Override
    public void start() {
        golem.setTarget(potentialTarget);
        super.start();
    }
}
