// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.FluidTags;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;

public class TryFindLand {
    private static final int COOLDOWN_TICKS = 60;

    public static BehaviorControl<PathfinderMob> create(int range, float speedModifier) {
        long[] nextOkStartTime = {0L};
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET) || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            if (!level.getFluidState(body.blockPosition()).is(FluidTags.WATER)) {
                return false;
            }
            if (timestamp < nextOkStartTime[0]) {
                nextOkStartTime[0] = timestamp + COOLDOWN_TICKS;
                return true;
            }
            BlockPos origin = body.blockPosition();
            CollisionContext context = CollisionContext.of(body);
            for (BlockPos pos : BlockPos.betweenClosed(origin.offset(-range, -range, -range), origin.offset(range, range, range))) {
                if (Math.abs(pos.getX() - origin.getX()) + Math.abs(pos.getY() - origin.getY()) + Math.abs(pos.getZ() - origin.getZ()) > range
                    || pos.getX() == origin.getX() && pos.getZ() == origin.getZ()) {
                    continue;
                }
                BlockState state = level.getBlockState(pos);
                if (state.getFluidState().isEmpty() && canStandOn(level, pos, state, context)) {
                    BlockPos target = pos.immutable();
                    brain.setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(target));
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new BlockPosTracker(target), speedModifier, 1));
                    break;
                }
            }
            nextOkStartTime[0] = timestamp + COOLDOWN_TICKS;
            return true;
        });
    }

    private static boolean canStandOn(ServerLevel level, BlockPos pos, BlockState state, CollisionContext context) {
        return state.getCollisionShape(level, pos, context).isEmpty() && level.getBlockState(pos.below()).isFaceSturdy(level, pos.below(), Direction.UP);
    }
}
