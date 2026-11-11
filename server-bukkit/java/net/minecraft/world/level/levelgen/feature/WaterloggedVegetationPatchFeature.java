// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.placement.CaveSurface;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class WaterloggedVegetationPatchFeature extends VegetationPatchFeature {
    private static boolean open(WorldGenLevel level, BlockPos pos, BlockPos.MutableBlockPos probe, Direction side) {
        probe.setWithOffset(pos, side);
        return !level.getBlockState(probe).isFaceSturdy(level, probe, side.getOpposite());
    }
    private static boolean leaks(WorldGenLevel level, BlockPos pos, BlockPos.MutableBlockPos probe) {
        return open(level, pos, probe, Direction.NORTH) || open(level, pos, probe, Direction.EAST) || open(level, pos, probe, Direction.SOUTH)
            || open(level, pos, probe, Direction.WEST) || open(level, pos, probe, Direction.DOWN);
    }
    public static final MapCodec<WaterloggedVegetationPatchFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.WaterloggedVegetationPatchFeature.class, "CODEC");
    public WaterloggedVegetationPatchFeature(HolderSet<Block> replaceable, Holder<BlockStateProvider> groundState, Holder<PlacedFeature> vegetationFeature, CaveSurface surface, IntProvider depth, float extraBottomBlockChance, int verticalRange, float vegetationChance, IntProvider xzRadius, float extraEdgeColumnChance) {
        super(replaceable, groundState, vegetationFeature, surface, depth, extraBottomBlockChance, verticalRange, vegetationChance, xzRadius, extraEdgeColumnChance);
    }
    public MapCodec<WaterloggedVegetationPatchFeature> codec() {
        return CODEC;
    }
    public Set<BlockPos> placeGroundPatch(WorldGenLevel level, RandomSource random, BlockPos origin, Predicate<BlockState> replaceable, int xRadius, int zRadius) {
        Set<BlockPos> ground = super.placeGroundPatch(level, random, origin, replaceable, xRadius, zRadius);
        Set<BlockPos> wet = new java.util.HashSet<>();
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        for (BlockPos spot : ground) {
            if (!leaks(level, spot, probe)) {
                wet.add(spot);
            }
        }
        for (BlockPos spot : wet) {
            level.setBlock(spot, Blocks.WATER.defaultBlockState(), 2);
        }
        return wet;
    }
    protected boolean placeVegetation(WorldGenLevel level, ChunkGenerator generator, RandomSource random, BlockPos placementPos) {
        if (!super.placeVegetation(level, generator, random, placementPos.below())) {
            return false;
        }
        BlockState placed = level.getBlockState(placementPos);
        if (placed.hasProperty(BlockStateProperties.WATERLOGGED) && !placed.getValue(BlockStateProperties.WATERLOGGED)) {
            level.setBlock(placementPos, placed.setValue(BlockStateProperties.WATERLOGGED, true), 2);
        }
        return true;


    }
}
