// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTest;
public class ScatteredOreFeature extends AbstractOreFeature {
    public static final MapCodec<ScatteredOreFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.ScatteredOreFeature.class, "CODEC");
    public ScatteredOreFeature(List<BlockReplacement> targetStates, int size, float discardChanceOnAirExposure) {
        super(targetStates, size, discardChanceOnAirExposure);
    }
    public ScatteredOreFeature(RuleTest target, BlockState state, int size, float discardChanceOnAirExposure) {
        this(List.of(BlockReplacement.replace(target, state)), size, discardChanceOnAirExposure);
    }
    public MapCodec<ScatteredOreFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        int tries = random.nextInt(size + 1);
        BlockPos.MutableBlockPos spot = new BlockPos.MutableBlockPos();
        for (int i = 0; i < tries; i++) {
            int reach = Math.min(i, 7);
            int dx = Math.round((random.nextFloat() - random.nextFloat()) * (float) reach);
            int dy = Math.round((random.nextFloat() - random.nextFloat()) * (float) reach);
            int dz = Math.round((random.nextFloat() - random.nextFloat()) * (float) reach);
            spot.setWithOffset(origin, dx, dy, dz);
            BlockState current = level.getBlockState(spot);
            for (BlockReplacement entry : targetStates) {
                if (canPlaceOre(current, level::getBlockState, random, entry, spot)) {
                    level.setBlock(spot, entry.state(), 2);
                    break;
                }
            }
        }
        return true;
    }
}
