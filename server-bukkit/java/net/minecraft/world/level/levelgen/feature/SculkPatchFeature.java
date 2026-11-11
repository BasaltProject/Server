// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.SculkSpreader;
import net.minecraft.world.level.block.SculkBehaviour;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record SculkPatchFeature(int chargeCount, int amountPerCharge, int spreadAttempts, int growthRounds, int spreadRounds) implements Feature {
    private boolean canStart(LevelAccessor level, BlockPos origin) {
        BlockState start = level.getBlockState(origin);
        if (start.getBlock() instanceof SculkBehaviour) {
            return true;
        }
        if (!start.isAir() && (!start.is(Blocks.WATER) || !start.getFluidState().isSource())) {
            return false;
        }
        return Direction.stream().map(origin::relative).anyMatch(p -> level.getBlockState(p).isCollisionShapeFullBlock(level, p));
    }


    public static final MapCodec<SculkPatchFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SculkPatchFeature.class, "CODEC");
    public MapCodec<SculkPatchFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!canStart(level, origin)) {
            return false;
        }
        SculkSpreader spreader = SculkSpreader.createWorldGenSpreader();
        int rounds = spreadRounds + growthRounds;
        for (int round = 0; round < rounds; round++) {
            for (int c = 0; c < chargeCount; c++) {
                spreader.addCursors(origin, amountPerCharge);
            }
            boolean veins = round < spreadRounds;
            for (int a = 0; a < spreadAttempts; a++) {
                spreader.updateCursors(level, origin, random, veins);
            }
            spreader.clear();
        }
        return true;
    }
}
