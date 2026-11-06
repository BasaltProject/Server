// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelSimulatedReader;
import net.minecraft.world.level.LevelWriter;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.featuresize.FeatureSize;
import net.minecraft.world.level.levelgen.feature.foliageplacers.FoliagePlacer;
import net.minecraft.world.level.levelgen.feature.rootplacers.RootPlacer;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.treedecorators.TreeDecorator;
import net.minecraft.world.level.levelgen.feature.trunkplacers.TrunkPlacer;
public record TreeFeature(Holder trunkProvider, Object trunkPlacer, Holder foliageProvider, Object foliagePlacer, Optional rootPlacer, Object minimumSize, List decorators, boolean ignoreVines, Holder belowTrunkProvider) implements Feature {
    public static final MapCodec<TreeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.TreeFeature.class, "CODEC");
    public MapCodec<TreeFeature> codec() {
        return CODEC;
    }
    public static boolean isVine(LevelSimulatedReader level, BlockPos pos) {
        return level.isStateAtPosition(pos, state -> state.is(net.minecraft.world.level.block.Blocks.VINE));
    }
    public static boolean isAirOrLeaves(LevelSimulatedReader level, BlockPos pos) {
        return level.isStateAtPosition(pos, state -> state.isAir() || state.is(net.minecraft.tags.BlockTags.LEAVES));
    }
    public static boolean validTreePos(LevelSimulatedReader level, BlockPos pos) {
        return level.isStateAtPosition(pos, state -> state.isAir() || state.is(net.minecraft.tags.BlockTags.REPLACEABLE_BY_TREES));
    }
    public void setBlock(LevelWriter level, BlockPos pos, BlockState blockState) {
        level.setBlock(pos, blockState, 19);
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider provider =
            (net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider) trunkProvider.value();
        String wood = org.basaltmc.bukkit.facade.NativeFeatures.woodOf(provider.getState(level, random, origin));
        String tree = wood == null ? null : switch (wood) {
            case "jungle" -> "jungle_tree";
            case "oak", "birch", "spruce", "acacia", "dark_oak", "cherry", "mangrove", "pale_oak" -> wood;
            default -> null;
        };
        return tree != null && org.basaltmc.bukkit.facade.NativeFeatures.placeTree(level, tree, origin);
    }
    public static List<BlockPos> getLowestTrunkOrRootOfTree(TreeDecorator.Context context) {
        List<BlockPos> positions = new java.util.ArrayList<>();
        var rootList = context.roots();
        var logList = context.logs();
        if (rootList.isEmpty()) {
            positions.addAll(logList);
        } else if (!logList.isEmpty() && rootList.get(0).getY() == logList.get(0).getY()) {
            positions.addAll(logList);
            positions.addAll(rootList);
        } else {
            positions.addAll(rootList);
        }
        return positions;
    }
    public static class Builder {
        public final Holder<BlockStateProvider> trunkProvider;
        private final TrunkPlacer trunkPlacer;
        public final Holder<BlockStateProvider> foliageProvider;
        private final FoliagePlacer foliagePlacer;
        private final Optional<RootPlacer> rootPlacer;
        private Holder<BlockStateProvider> belowTrunkProvider;
        private final FeatureSize minimumSize;
        private List<TreeDecorator> decorators = List.of();
        private boolean ignoreVines;

        public Builder(Holder<BlockStateProvider> trunkProvider, TrunkPlacer trunkPlacer, Holder<BlockStateProvider> foliageProvider, FoliagePlacer foliagePlacer, Optional<RootPlacer> rootPlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            this.trunkProvider = trunkProvider;
            this.trunkPlacer = trunkPlacer;
            this.foliageProvider = foliageProvider;
            this.foliagePlacer = foliagePlacer;
            this.rootPlacer = rootPlacer;
            this.minimumSize = minimumSize;
            this.belowTrunkProvider = belowTrunkProvider;
        }
        public Builder(BlockStateProvider trunkProvider, TrunkPlacer trunkPlacer, BlockStateProvider foliageProvider, FoliagePlacer foliagePlacer, Optional<RootPlacer> rootPlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            this(Holder.direct(trunkProvider), trunkPlacer, Holder.direct(foliageProvider), foliagePlacer, rootPlacer, minimumSize, belowTrunkProvider);
        }
        public Builder(BlockStateProvider trunkProvider, TrunkPlacer trunkPlacer, BlockStateProvider foliageProvider, FoliagePlacer foliagePlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            this(trunkProvider, trunkPlacer, foliageProvider, foliagePlacer, Optional.empty(), minimumSize, belowTrunkProvider);
        }
        public Builder belowTrunkProvider(Holder<BlockStateProvider> belowTrunkProvider) {
            this.belowTrunkProvider = belowTrunkProvider;
            return this;
        }
        public Builder decorators(List<TreeDecorator> decorators) {
            this.decorators = decorators;
            return this;
        }
        public Builder ignoreVines() {
            this.ignoreVines = true;
            return this;
        }
        public TreeFeature build() {
            return new TreeFeature(trunkProvider, trunkPlacer, foliageProvider, foliagePlacer, rootPlacer, minimumSize, decorators, ignoreVines, belowTrunkProvider);
        }
    }
}
