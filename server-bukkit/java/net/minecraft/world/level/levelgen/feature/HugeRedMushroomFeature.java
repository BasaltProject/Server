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
public record HugeRedMushroomFeature(Holder<BlockStateProvider> capProvider, Holder<BlockStateProvider> stemProvider, int foliageRadius, BlockPredicate canPlaceOn) implements AbstractHugeMushroomFeature {
    public static final MapCodec<HugeRedMushroomFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.HugeRedMushroomFeature.class, "CODEC");
    public MapCodec<HugeRedMushroomFeature> codec() {
        return CODEC;
    }
    public void makeCap(WorldGenLevel level, RandomSource random, BlockPos origin, int treeHeight, BlockPos.MutableBlockPos blockPos) {
        for (int dy = treeHeight - 3; dy <= treeHeight; dy++) {
            int reach = dy < treeHeight ? foliageRadius : foliageRadius - 1;
            int inner = foliageRadius - 2;
            for (int dx = -reach; dx <= reach; dx++) {
                for (int dz = -reach; dz <= reach; dz++) {
                    boolean edgeX = dx == -reach || dx == reach;
                    boolean edgeZ = dz == -reach || dz == reach;
                    if (dy < treeHeight && edgeX == edgeZ) {
                        continue;
                    }
                    blockPos.setWithOffset(origin, dx, dy, dz);
                    BlockState cap = capProvider.value().getState(level, random, origin);
                    if (cap.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.WEST) && cap.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.EAST)
                        && cap.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.NORTH) && cap.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.SOUTH)
                        && cap.hasProperty(net.minecraft.world.level.block.HugeMushroomBlock.UP)) {
                        cap = cap.setValue(net.minecraft.world.level.block.HugeMushroomBlock.UP, dy >= treeHeight - 1)
                            .setValue(net.minecraft.world.level.block.HugeMushroomBlock.WEST, dx < -inner)
                            .setValue(net.minecraft.world.level.block.HugeMushroomBlock.EAST, dx > inner)
                            .setValue(net.minecraft.world.level.block.HugeMushroomBlock.NORTH, dz < -inner)
                            .setValue(net.minecraft.world.level.block.HugeMushroomBlock.SOUTH, dz > inner);
                    }
                    placeMushroomBlock(level, blockPos, cap);
                }
            }
        }
    }
    public int getTreeRadiusForHeight(int trunkHeight, int treeHeight, int leafRadius, int yo) {
        return (yo < treeHeight && yo >= treeHeight - 3) || yo == treeHeight ? leafRadius : 0;


    }
}
