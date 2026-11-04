// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.SectionPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.phys.Vec3;

public class MoveBackToVillageGoal extends RandomStrollGoal {
    public MoveBackToVillageGoal(PathfinderMob mob, double speedModifier, boolean checkNoActionTime) {
        super(mob, speedModifier, 10, checkNoActionTime);
    }

    @Override
    public boolean canUse() {
        return !((ServerLevel) mob.level()).isVillage(mob.blockPosition()) && super.canUse();
    }

    @Override
    protected Vec3 getPosition() {
        ServerLevel level = (ServerLevel) mob.level();
        SectionPos here = SectionPos.of(mob.blockPosition());
        SectionPos closest = closestToVillage(level, here);
        return closest != here
                ? DefaultRandomPos.getPosTowards(mob, 10, 7, Vec3.atBottomCenterOf(closest.center()), Math.PI / 2)
                : null;
    }

    private static SectionPos closestToVillage(ServerLevel level, SectionPos here) {
        int current = level.sectionsToVillage(here);
        return SectionPos.cube(here, 2).filter(section -> level.sectionsToVillage(section) < current)
                .min(java.util.Comparator.comparingInt(level::sectionsToVillage)).orElse(here);
    }
}
