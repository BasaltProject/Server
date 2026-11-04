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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#codec");
    }
    public static boolean isVine(LevelSimulatedReader level, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#isVine");
    }
    public static boolean isAirOrLeaves(LevelSimulatedReader level, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#isAirOrLeaves");
    }
    public static boolean validTreePos(LevelSimulatedReader level, BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#validTreePos");
    }
    public void setBlock(LevelWriter level, BlockPos pos, BlockState blockState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#setBlock");
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#place");
    }
    public static List<BlockPos> getLowestTrunkOrRootOfTree(TreeDecorator.Context context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature#getLowestTrunkOrRootOfTree");
    }
    public static class Builder {
        public final Holder<BlockStateProvider> trunkProvider = null;
        public final Holder<BlockStateProvider> foliageProvider = null;
        public Builder(Holder<BlockStateProvider> trunkProvider, TrunkPlacer trunkPlacer, Holder<BlockStateProvider> foliageProvider, FoliagePlacer foliagePlacer, Optional<RootPlacer> rootPlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#<init>");
        }
        public Builder(BlockStateProvider trunkProvider, TrunkPlacer trunkPlacer, BlockStateProvider foliageProvider, FoliagePlacer foliagePlacer, Optional<RootPlacer> rootPlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#<init>");
        }
        public Builder(BlockStateProvider trunkProvider, TrunkPlacer trunkPlacer, BlockStateProvider foliageProvider, FoliagePlacer foliagePlacer, FeatureSize minimumSize, Holder<BlockStateProvider> belowTrunkProvider) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#<init>");
        }
        public Builder belowTrunkProvider(Holder<BlockStateProvider> belowTrunkProvider) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#belowTrunkProvider");
        }
        public Builder decorators(List<TreeDecorator> decorators) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#decorators");
        }
        public Builder ignoreVines() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#ignoreVines");
        }
        public TreeFeature build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.TreeFeature.Builder#build");
        }
    }
}
