// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class SetWalkTargetAwayFrom {
    public static BehaviorControl<PathfinderMob> pos(MemoryModuleType<BlockPos> memory, float speedModifier, int desiredDistance, boolean interruptCurrentWalk) {
        return awayFrom(memory, speedModifier, desiredDistance, interruptCurrentWalk, pos -> Vec3.atBottomCenterOf(pos));
    }

    public static OneShot<PathfinderMob> entity(MemoryModuleType<? extends Entity> memory, float speedModifier, int desiredDistance, boolean interruptCurrentWalk) {
        return awayFrom(memory, speedModifier, desiredDistance, interruptCurrentWalk, Entity::position);
    }

    private static <T> OneShot<PathfinderMob> awayFrom(MemoryModuleType<? extends T> memory, float speedModifier, int desiredDistance,
            boolean interruptCurrentWalk, java.util.function.Function<T, Vec3> position) {
        return OneShots.of(Set.of(MemoryModuleType.WALK_TARGET, memory), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!interruptCurrentWalk && brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            var value = brain.getMemory(memory);
            if (value.isEmpty()) {
                return false;
            }
            Vec3 from = position.apply(value.get());
            if (body.position().closerThan(from, desiredDistance)) {
                return false;
            }
            Vec3 away = LandRandomPos.getPosAway(body, desiredDistance, 7, from);
            if (away == null) {
                return false;
            }
            brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(away, speedModifier, 0));
            return true;
        });
    }
}
