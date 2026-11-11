// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public interface ChangeOverTimeBlock<T extends Enum<T>> {
    int SCAN_DISTANCE = 4;
    Optional<BlockState> getNext(BlockState state);
    float getChanceModifier();
    default void changeOverTime(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        if (random.nextFloat() < 0.05688889F) {
            getNextState(state, level, pos, random).ifPresent(weathered -> level.setBlockAndUpdate(pos, weathered));
        }
    }
    T getAge();
    default Optional<BlockState> getNextState(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        int ownAge = getAge().ordinal();
        int same = 0;
        int older = 0;
        for (BlockPos near : BlockPos.withinManhattan(pos, 4)) {
            if (near.equals(pos) || !(level.getBlockState(near).getBlock() instanceof ChangeOverTimeBlock<?> other)
                || other.getAge().getClass() != getAge().getClass()) {
                continue;
            }
            int foundAge = other.getAge().ordinal();
            if (foundAge < ownAge) {
                return Optional.empty();
            }
            if (foundAge > ownAge) {
                older++;
            } else {
                same++;
            }
        }
        float chance = (float) (older + 1) / (float) (older + same + 1);
        return random.nextFloat() < chance * chance * getChanceModifier() ? getNext(state) : Optional.empty();


    }
}
