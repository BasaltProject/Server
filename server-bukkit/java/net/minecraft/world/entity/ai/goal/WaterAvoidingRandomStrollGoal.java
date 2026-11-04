// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class WaterAvoidingRandomStrollGoal extends RandomStrollGoal {
    public static final float PROBABILITY = 0.001F;
    protected final float probability;

    public WaterAvoidingRandomStrollGoal(PathfinderMob mob, double speedModifier) {
        this(mob, speedModifier, PROBABILITY);
    }

    public WaterAvoidingRandomStrollGoal(PathfinderMob mob, double speedModifier, float probability) {
        super(mob, speedModifier);
        this.probability = probability;
    }

    @Override
    protected Vec3 getPosition() {
        if (mob.isInWater()) {
            Vec3 land = LandRandomPos.getPos(mob, 15, 7);
            return land == null ? super.getPosition() : land;
        }
        return mob.getRandom().nextFloat() >= probability ? LandRandomPos.getPos(mob, 10, 7) : super.getPosition();
    }
}
