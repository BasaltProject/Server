// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Block;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record ReplaceBlobsFeature(BlockState targetState, BlockState replaceState, IntProvider radius) implements Feature {
    private static BlockPos seek(LevelAccessor level, BlockPos.MutableBlockPos cursor, Block wanted) {
        while (cursor.getY() > level.getMinY() + 1) {
            if (level.getBlockState(cursor).is(wanted)) {
                return cursor;
            }
            cursor.move(Direction.DOWN);
        }
        return null;
    }
    public static final MapCodec<ReplaceBlobsFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.ReplaceBlobsFeature.class, "CODEC");
    public MapCodec<ReplaceBlobsFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Block wanted = targetState.getBlock();
        BlockPos centre = seek(level, origin.mutable().clamp(Direction.Axis.Y, level.getMinY() + 1, level.getMaxY()), wanted);
        if (centre == null) {
            return false;
        }
        int rx = radius.sample(random);
        int ry = radius.sample(random);
        int rz = radius.sample(random);
        int limit = Math.max(rx, Math.max(ry, rz));
        boolean any = false;
        for (BlockPos spot : BlockPos.withinBoxByManhattanDistance(centre, rx, ry, rz)) {
            if (spot.distManhattan(centre) > limit) {
                break;
            }
            if (level.getBlockState(spot).is(wanted)) {
                setBlock(level, spot, replaceState);
                any = true;
            }
        }
        return any;
    }
}
