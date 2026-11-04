// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.block.LiquidBlock;
import net.minecraft.world.level.material.Fluid;

public class TryFindLiquid {
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
                nextOkStartTime[0] = timestamp + 22L;
                return true;
            }
            BlockPos origin = body.blockPosition();
            BlockPos found = null;
            search:
            for (BlockPos pos : BlockPos.betweenClosed(origin.offset(-range, -range, -range), origin.offset(range, range, range))) {
                if (Math.abs(pos.getX() - origin.getX()) + Math.abs(pos.getY() - origin.getY()) + Math.abs(pos.getZ() - origin.getZ()) > range
                    || pos.getX() == origin.getX() && pos.getZ() == origin.getZ()) {
                    continue;
                }
                var state = level.getBlockState(pos);
                if (!(state.getBlock() instanceof LiquidBlock) || !state.getFluidState().is(fluidTag)) {
                    continue;
                }
                if (level.getBlockState(pos.above()).isAir()) {
                    found = pos.immutable();
                    break search;
                }
                if (found == null && !pos.closerToCenterThan(body.position(), 1.5)) {
                    found = pos.immutable();
                }
            }
            if (found != null) {
                brain.setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(found));
                brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new BlockPosTracker(found), speedModifier, 0));
            }
            nextOkStartTime[0] = timestamp + 40L;
            return true;
        });
    }
}
