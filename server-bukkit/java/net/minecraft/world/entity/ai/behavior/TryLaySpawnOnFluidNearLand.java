// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.BlockTags;
import net.minecraft.tags.FluidTags;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class TryLaySpawnOnFluidNearLand {
    public static BehaviorControl<LivingEntity> create(Block spawnBlock) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.ATTACK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.IS_PREGNANT), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.ATTACK_TARGET) || !brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)
                || !brain.hasMemoryValue(MemoryModuleType.IS_PREGNANT) || body.isInWater() || !body.onGround()) {
                return false;
            }
            BlockPos below = body.blockPosition().below();
            for (Direction direction : Direction.Plane.HORIZONTAL) {
                BlockPos relative = below.relative(direction);
                if (level.getBlockState(relative).getCollisionShape(level, relative).getFaceShape(Direction.UP).isEmpty()
                    && (level.getFluidState(relative).is(FluidTags.SUPPORTS_FROGSPAWN) || level.getBlockState(relative).is(BlockTags.SUPPORTS_FROGSPAWN))) {
                    BlockPos spawnPos = relative.above();
                    if (level.getBlockState(spawnPos).isAir()) {
                        BlockState state = spawnBlock.defaultBlockState();
                        level.setBlockAndUpdate(spawnPos, state);
                        level.playSound(null, body, SoundEvents.FROG_LAY_SPAWN, SoundSource.BLOCKS, 1.0F, 1.0F);
                        brain.eraseMemory(MemoryModuleType.IS_PREGNANT);
                        return true;
                    }
                }
            }
            return true;
        });
    }
}
