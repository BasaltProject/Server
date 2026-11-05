// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.blockscan;

import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Continuation;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunkSection;

public class BlockScanUtils {
    static boolean findBlocks(LevelReader level, BlockPos from, BlockPos to, Predicate<BlockState> predicate, BlockStateConsumer consumer) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int y = from.getY(); y <= to.getY(); y++) {
            for (int z = from.getZ(); z <= to.getZ(); z++) {
                for (int x = from.getX(); x <= to.getX(); x++) {
                    cursor.set(x, y, z);
                    if (!level.hasChunkAt(cursor)) {
                        continue;
                    }
                    BlockState state = level.getBlockState(cursor);
                    if (predicate.test(state) && consumer.apply(cursor, state) == Continuation.ABORT) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    public static boolean findBlocksInSection(LevelChunkSection section, BlockPos origin, int fromX, int fromY, int fromZ, int toX, int toY, int toZ, Predicate<BlockState> predicate, BlockStateConsumer consumer) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int y = fromY; y <= toY; y++) {
            for (int z = fromZ; z <= toZ; z++) {
                for (int x = fromX; x <= toX; x++) {
                    BlockState state = section.getBlockState(x, y, z);
                    if (!predicate.test(state)) {
                        continue;
                    }
                    cursor.set(origin.getX() + x, origin.getY() + y, origin.getZ() + z);
                    if (consumer.apply(cursor, state) == Continuation.ABORT) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    static boolean findBlocksWithCache(LevelReader level, Iterable<BlockPos> positions, Predicate<BlockState> predicate, BlockStateConsumer consumer) {
        for (BlockPos pos : positions) {
            if (!level.hasChunkAt(pos)) {
                continue;
            }
            BlockState state = level.getBlockState(pos);
            if (predicate.test(state) && consumer.apply(pos, state) == Continuation.ABORT) {
                return false;
            }
        }
        return true;
    }
}
