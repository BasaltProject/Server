// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.phys.shapes.CollisionContext;

public class TryFindLandNearLiquid {
    public static BehaviorControl<PathfinderMob> create(int range, float speedModifier, TagKey<Fluid> fluidTag) {
        long[] nextOkStartTime = {0L};
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET) || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            if (level.getFluidState(body.blockPosition()).is(fluidTag)) {
                return false;
            }
            if (timestamp < nextOkStartTime[0]) {
                nextOkStartTime[0] = timestamp + 40L;
                return true;
            }
            CollisionContext context = CollisionContext.of(body);
            BlockPos origin = body.blockPosition();
            search:
            for (BlockPos pos : BlockPos.betweenClosed(origin.offset(-range, -range, -range), origin.offset(range, range, range))) {
                if (Math.abs(pos.getX() - origin.getX()) + Math.abs(pos.getY() - origin.getY()) + Math.abs(pos.getZ() - origin.getZ()) > range
                    || pos.getX() == origin.getX() && pos.getZ() == origin.getZ()) {
                    continue;
                }
                if (!level.getBlockState(pos).getCollisionShape(level, pos, context).isEmpty()
                    || level.getBlockState(pos.below()).getCollisionShape(level, pos, context).isEmpty()) {
                    continue;
                }
                for (Direction direction : Direction.Plane.HORIZONTAL) {
                    BlockPos side = pos.relative(direction);
                    if (level.getBlockState(side).isAir() && level.getBlockState(side.below()).getFluidState().is(fluidTag)) {
                        BlockPos target = pos.immutable();
                        brain.setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(target));
                        brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new BlockPosTracker(target), speedModifier, 0));
                        break search;
                    }
                }
            }
            nextOkStartTime[0] = timestamp + 40L;
            return true;
        });
    }
}
