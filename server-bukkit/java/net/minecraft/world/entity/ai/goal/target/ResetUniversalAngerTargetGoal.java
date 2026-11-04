// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.List;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.phys.AABB;

public class ResetUniversalAngerTargetGoal<T extends Mob & NeutralMob> extends Goal {
    private static final int ALERT_RANGE_Y = 10;
    private final T mob;
    private final boolean alertOthersOfSameType;
    private int lastHurtByPlayerTimestamp;

    public ResetUniversalAngerTargetGoal(T mob, boolean alertOthersOfSameType) {
        this.mob = mob;
        this.alertOthersOfSameType = alertOthersOfSameType;
    }

    @Override
    public boolean canUse() {
        ServerLevel level = getServerLevel(mob);
        return Boolean.TRUE.equals(level.getWorld().getGameRuleValue(org.bukkit.GameRule.UNIVERSAL_ANGER))
                && wasHurtByPlayer();
    }

    private boolean wasHurtByPlayer() {
        return mob.getLastHurtByMob() != null && mob.getLastHurtByMob().getType() == net.minecraft.world.entity.EntityTypes.PLAYER
                && mob.getLastHurtByMobTimestamp() != lastHurtByPlayerTimestamp;
    }

    @Override
    public void start() {
        lastHurtByPlayerTimestamp = mob.getLastHurtByMobTimestamp();
        mob.forgetCurrentTargetAndRefreshUniversalAnger();
        if (alertOthersOfSameType) {
            double range = mob.getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE);
            AABB area = AABB.unitCubeFromLowerCorner(mob.position()).inflate(range, ALERT_RANGE_Y, range);
            List<? extends Mob> others = mob.level().getEntitiesOfClass(mob.getClass(), area, other -> other != mob);
            for (Mob other : others) {
                if (other instanceof NeutralMob neutral) {
                    neutral.forgetCurrentTargetAndRefreshUniversalAnger();
                }
            }
        }
        super.start();
    }
}
