// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import java.util.stream.IntStream;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.util.Util;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record BonusChestFeature() implements Feature {
    public static final MapCodec<BonusChestFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BonusChestFeature.class, "CODEC");
    public MapCodec<BonusChestFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        ChunkPos chunk = ChunkPos.containing(origin);
        IntArrayList xs = Util.toShuffledList(IntStream.rangeClosed(chunk.getMinBlockX(), chunk.getMaxBlockX()), random);
        IntArrayList zs = Util.toShuffledList(IntStream.rangeClosed(chunk.getMinBlockZ(), chunk.getMaxBlockZ()), random);
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int x : xs) {
            for (int z : zs) {
                cursor.set(x, 0, z);
                BlockPos spot = level.getHeightmapPos(Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, cursor);
                if (!level.isEmptyBlock(spot) && !level.getBlockState(spot).getCollisionShape(level, spot).isEmpty()) {
                    continue;
                }
                level.setBlock(spot, Blocks.CHEST.defaultBlockState(), 2);
                if (level.getBlockEntity(spot) instanceof RandomizableContainer container) {
                    container.setLootTable(BuiltInLootTables.SPAWN_BONUS_CHEST, random.nextLong());
                }
                BlockState torch = Blocks.TORCH.defaultBlockState();
                for (Direction side : Direction.Plane.HORIZONTAL) {
                    BlockPos beside = spot.relative(side);
                    if (torch.canSurvive(level, beside)) {
                        level.setBlock(beside, torch, 2);
                    }
                }
                return true;
            }
        }
        return false;
    }
}
