// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record BambooFeature(float probability) implements Feature {
    private static BlockState stalk(String leaves, int stage) {
        BlockState base = Blocks.BAMBOO.defaultBlockState();
        base = org.basaltmc.bukkit.facade.StateProps.with(base, "age", 1);
        base = org.basaltmc.bukkit.facade.StateProps.with(base, "leaves", leaves);
        return org.basaltmc.bukkit.facade.StateProps.with(base, "stage", stage);
    }
    public static final MapCodec<BambooFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.BambooFeature.class, "CODEC");
    public MapCodec<BambooFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockState trunk = stalk("none", 0);
        BlockPos.MutableBlockPos stem = origin.mutable();
        BlockPos.MutableBlockPos ground = origin.mutable();
        if (!level.isEmptyBlock(stem)) {
            return false;
        }
        if (Blocks.BAMBOO.defaultBlockState().canSurvive(level, stem)) {
            int tall = random.nextInt(12) + 5;
            if (random.nextFloat() < probability) {
                int reach = random.nextInt(4) + 1;
                for (int x = origin.getX() - reach; x <= origin.getX() + reach; x++) {
                    for (int z = origin.getZ() - reach; z <= origin.getZ() + reach; z++) {
                        int ox = x - origin.getX();
                        int oz = z - origin.getZ();
                        if (ox * ox + oz * oz <= reach * reach) {
                            ground.set(x, level.getHeight(Heightmap.Types.WORLD_SURFACE, x, z) - 1, z);
                            if (level.getBlockState(ground).is(net.minecraft.tags.BlockTags.BENEATH_BAMBOO_PODZOL_REPLACEABLE)) {
                                level.setBlock(ground, Blocks.PODZOL.defaultBlockState(), 2);
                            }
                        }
                    }
                }
            }
            for (int i = 0; i < tall && level.isEmptyBlock(stem); i++) {
                level.setBlock(stem, trunk, 2);
                stem.move(Direction.UP, 1);
            }
            if (stem.getY() - origin.getY() >= 3) {
                level.setBlock(stem, stalk("large", 1), 2);
                level.setBlock(stem.move(Direction.DOWN, 1), stalk("large", 0), 2);
                level.setBlock(stem.move(Direction.DOWN, 1), stalk("small", 0), 2);
            }
        }
        return true;
    }
}
