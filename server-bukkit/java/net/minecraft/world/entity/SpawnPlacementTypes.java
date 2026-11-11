// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.FluidTags;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.NaturalSpawner;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.PathComputationType;
public interface SpawnPlacementTypes {
    SpawnPlacementType NO_RESTRICTIONS = (level, pos, type) -> true;
    SpawnPlacementType IN_WATER = (level, pos, type) -> type != null && level.getWorldBorder().isWithinBounds(pos)
        && level.getFluidState(pos).is(FluidTags.WATER) && !level.getBlockState(pos.above()).isRedstoneConductor(level, pos.above());
    SpawnPlacementType IN_LAVA = (level, pos, type) -> type != null && level.getWorldBorder().isWithinBounds(pos)
        && level.getFluidState(pos).is(FluidTags.LAVA);
    SpawnPlacementType ON_GROUND = new SpawnPlacementType() {
        public boolean isSpawnPositionOk(LevelReader level, BlockPos pos, EntityType<?> type) {
            if (type == null || !level.getWorldBorder().isWithinBounds(pos)) {
                return false;
            }
            BlockPos below = pos.below();
            return level.getBlockState(below).isValidSpawn(level, below, type) && emptyAt(level, pos, type) && emptyAt(level, pos.above(), type);
        }
        private boolean emptyAt(LevelReader level, BlockPos pos, EntityType<?> type) {
            BlockState state = level.getBlockState(pos);
            return NaturalSpawner.isValidEmptySpawnBlock(level, pos, state, state.getFluidState(), type);
        }
        public BlockPos adjustSpawnPosition(LevelReader level, BlockPos candidate) {
            BlockPos below = candidate.below();
            return level.getBlockState(below).isPathfindable(PathComputationType.LAND) ? below : candidate;
        }
    };
}
