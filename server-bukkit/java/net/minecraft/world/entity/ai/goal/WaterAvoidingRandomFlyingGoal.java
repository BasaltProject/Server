// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.AirAndWaterRandomPos;
import net.minecraft.world.entity.ai.util.HoverRandomPos;
import net.minecraft.world.phys.Vec3;

public class WaterAvoidingRandomFlyingGoal extends WaterAvoidingRandomStrollGoal {
    public WaterAvoidingRandomFlyingGoal(PathfinderMob mob, double speedModifier) {
        super(mob, speedModifier);
    }

    @Override
    protected Vec3 getPosition() {
        Vec3 view = mob.getViewVector(0.0F);
        Vec3 pos = HoverRandomPos.getPos(mob, 8, 7, view.x, view.z, (float) (Math.PI / 2), 3, 1);
        return pos != null ? pos : AirAndWaterRandomPos.getPos(mob, 8, 4, -2, view.x, view.z, (float) (Math.PI / 2));
    }
}
