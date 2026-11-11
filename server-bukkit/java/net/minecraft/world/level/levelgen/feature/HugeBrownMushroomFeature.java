// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record HugeBrownMushroomFeature(Holder<BlockStateProvider> capProvider, Holder<BlockStateProvider> stemProvider, int foliageRadius, BlockPredicate canPlaceOn) implements AbstractHugeMushroomFeature {
    private static boolean hasSides(BlockState s) {
        return s.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.WEST) && s.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.EAST)
            && s.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.NORTH) && s.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.SOUTH);
    }
    public static final MapCodec<HugeBrownMushroomFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.HugeBrownMushroomFeature.class, "CODEC");
    public MapCodec<HugeBrownMushroomFeature> codec() {
        return CODEC;
    }
    public void makeCap(WorldGenLevel level, RandomSource random, BlockPos origin, int treeHeight, BlockPos.MutableBlockPos blockPos) {
        for (int dx = -foliageRadius; dx <= foliageRadius; dx++) {
            for (int dz = -foliageRadius; dz <= foliageRadius; dz++) {
                boolean loX = dx == -foliageRadius;
                boolean hiX = dx == foliageRadius;
                boolean loZ = dz == -foliageRadius;
                boolean hiZ = dz == foliageRadius;
                boolean edgeX = loX || hiX;
                boolean edgeZ = loZ || hiZ;
                if (edgeX && edgeZ) {
                    continue;
                }
                blockPos.setWithOffset(origin, dx, treeHeight, dz);
                boolean west = loX || edgeZ && dx == 1 - foliageRadius;
                boolean east = hiX || edgeZ && dx == foliageRadius - 1;
                boolean north = loZ || edgeX && dz == 1 - foliageRadius;
                boolean south = hiZ || edgeX && dz == foliageRadius - 1;
                BlockState cap = capProvider.value().getState(level, random, origin);
                if (hasSides(cap)) {
                    cap = cap.setValue(net.minecraft.world.level.block.HugeMushroomBlock.WEST, west).setValue(net.minecraft.world.level.block.HugeMushroomBlock.EAST, east)
                        .setValue(net.minecraft.world.level.block.HugeMushroomBlock.NORTH, north).setValue(net.minecraft.world.level.block.HugeMushroomBlock.SOUTH, south);
                }
                placeMushroomBlock(level, blockPos, cap);
            }
        }
    }
    public int getTreeRadiusForHeight(int trunkHeight, int treeHeight, int leafRadius, int yo) {
        return yo <= 3 ? 0 : leafRadius;
    }
}
