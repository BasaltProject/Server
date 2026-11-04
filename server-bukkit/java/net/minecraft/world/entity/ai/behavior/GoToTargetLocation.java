// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class GoToTargetLocation {
    private static BlockPos getNearbyPos(Mob body, BlockPos pos) {
        RandomSource random = body.level().getRandom();
        return pos.offset(getRandomOffset(random), 0, getRandomOffset(random));
    }

    private static int getRandomOffset(RandomSource random) {
        return random.nextInt(3) - 1;
    }

    public static <E extends Mob> OneShot<E> create(MemoryModuleType<BlockPos> locationMemory, int closeEnoughDist, float speedModifier) {
        return OneShots.<E>of(Set.of(locationMemory, MemoryModuleType.ATTACK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            BlockPos location = brain.getMemory(locationMemory).orElse(null);
            if (location == null || brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET) || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            if (!location.closerThan(body.blockPosition(), closeEnoughDist)) {
                BehaviorUtils.setWalkAndLookTargetMemories(body, getNearbyPos(body, location), speedModifier, closeEnoughDist);
            }
            return true;
        });
    }
}
