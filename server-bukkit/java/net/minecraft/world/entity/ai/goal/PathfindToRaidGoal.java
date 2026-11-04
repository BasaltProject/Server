// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.entity.raid.Raid;
import net.minecraft.world.entity.raid.Raider;
import net.minecraft.world.entity.raid.Raids;
import net.minecraft.world.phys.Vec3;

public class PathfindToRaidGoal<T extends Raider> extends Goal {
    private static final int RECRUITMENT_SEARCH_TICK_DELAY = 20;
    private final T mob;
    private int recruitmentTick;

    public PathfindToRaidGoal(T mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        return mob.getTarget() == null && !mob.hasControllingPassenger() && mob.hasActiveRaid() && !mob.getCurrentRaid().isOver()
                && !getServerLevel(mob.level()).isVillage(mob.blockPosition());
    }

    @Override
    public boolean canContinueToUse() {
        return mob.hasActiveRaid() && !mob.getCurrentRaid().isOver() && !getServerLevel(mob.level()).isVillage(mob.blockPosition());
    }

    @Override
    public void tick() {
        if (!mob.hasActiveRaid()) {
            return;
        }
        Raid raid = mob.getCurrentRaid();
        if (mob.tickCount > recruitmentTick) {
            recruitmentTick = mob.tickCount + RECRUITMENT_SEARCH_TICK_DELAY;
            recruitNearby(raid);
        }
        if (!mob.isPathFinding()) {
            Vec3 towards = DefaultRandomPos.getPosTowards(mob, 15, 4, Vec3.atBottomCenterOf(raid.getCenter()), Math.PI / 2);
            if (towards != null) {
                mob.getNavigation().moveTo(towards.x, towards.y, towards.z, 1.0);
            }
        }
    }

    private void recruitNearby(Raid raid) {
        if (!raid.isActive()) {
            return;
        }
        ServerLevel level = getServerLevel(mob.level());
        for (Raider raider : level.getEntitiesOfClass(Raider.class, mob.getBoundingBox().inflate(16.0),
                other -> !other.hasActiveRaid() && Raids.canJoinRaid(other))) {
            raid.joinRaid(level, raid.getGroupsSpawned(), raider, (BlockPos) null, true);
        }
    }
}
