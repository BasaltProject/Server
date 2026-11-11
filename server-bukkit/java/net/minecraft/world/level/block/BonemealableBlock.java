// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
public interface BonemealableBlock {
    private static java.util.List<Direction> horizontals() {
        java.util.List<Direction> list = new java.util.ArrayList<>();
        for (Direction direction : Direction.Plane.HORIZONTAL) {
            list.add(direction);
        }
        return list;
    }
    private static Optional<BlockPos> spreadable(java.util.List<Direction> directions, LevelReader level, BlockPos pos, BlockState blockToPlace) {
        for (Direction direction : directions) {
            BlockPos neighbour = pos.relative(direction);
            if (level.isEmptyBlock(neighbour) && blockToPlace.canSurvive(level, neighbour)) {
                return Optional.of(neighbour);
            }
        }
        return Optional.empty();
    }
    boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source);
    boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source);
    void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source);
    static boolean hasSpreadableNeighbourPos(LevelReader level, BlockPos pos, BlockState blockToPlace) {
        return spreadable(horizontals(), level, pos, blockToPlace).isPresent();
    }
    static Optional<BlockPos> findSpreadableNeighbourPos(Level level, BlockPos pos, BlockState blockToPlace) {
        return spreadable(Direction.Plane.HORIZONTAL.shuffledCopy(level.getRandom()), level, pos, blockToPlace);
    }
    default BlockPos getParticlePos(BlockPos blockPos) {
        return getType() == Type.NEIGHBOR_SPREADER ? blockPos : blockPos.above();
    }
    default Type getType() {
        return Type.GROWER;
    }
    public static enum Type {
        NEIGHBOR_SPREADER, GROWER;
    }
}
