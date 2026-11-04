// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.util.AirAndWaterRandomPos;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class RandomStroll {
    private static final int MAX_XZ = 10;
    private static final int MAX_Y = 7;

    public static OneShot<PathfinderMob> stroll(float speedModifier) {
        return stroll(speedModifier, true);
    }

    public static OneShot<PathfinderMob> stroll(float speedModifier, boolean mayStrollFromWater) {
        return strollTo(speedModifier, mob -> mayStrollFromWater || !mob.isInWater() ? LandRandomPos.getPos(mob, MAX_XZ, MAX_Y) : null);
    }

    public static BehaviorControl<PathfinderMob> stroll(float speedModifier, int maxHorizontalDistance, int maxVerticalDistance) {
        return strollTo(speedModifier, mob -> LandRandomPos.getPos(mob, maxHorizontalDistance, maxVerticalDistance));
    }

    public static BehaviorControl<PathfinderMob> fly(float speedModifier) {
        return strollTo(speedModifier, mob -> AirAndWaterRandomPos.getPos(mob, MAX_XZ, MAX_Y, 0, mob.getViewVector(0.0F).x, mob.getViewVector(0.0F).z, 1.5707963D));
    }

    public static BehaviorControl<PathfinderMob> swim(float speedModifier) {
        return strollTo(speedModifier, mob -> LandRandomPos.getPos(mob, MAX_XZ, MAX_Y));
    }

    private static OneShot<PathfinderMob> strollTo(float speedModifier, java.util.function.Function<PathfinderMob, Vec3> finder) {
        return OneShots.of(Set.of(MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            Vec3 pos = finder.apply(body);
            if (pos == null) {
                return false;
            }
            body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(pos, speedModifier, 0));
            return true;
        });
    }
}
