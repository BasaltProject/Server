// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.rootplacers;
import com.mojang.datafixers.Products;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.LevelSimulatedReader;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public abstract class RootPlacer {
    public static final Codec<RootPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.rootplacers.RootPlacer.class, "CODEC");
    protected final IntProvider trunkOffsetY;
    protected final Holder<BlockStateProvider> rootProvider;
    protected final Optional<AboveRootPlacement> aboveRootPlacement;
    protected static <P extends RootPlacer> Products.P3<RecordCodecBuilder.Mu<P>, IntProvider, Holder<BlockStateProvider>, Optional<AboveRootPlacement>> rootPlacerParts(RecordCodecBuilder.Instance<P> instance) {
        return instance.group(net.minecraft.util.valueproviders.IntProviders.CODEC.fieldOf("trunk_offset_y").forGetter(c -> c.trunkOffsetY),
            BlockStateProvider.CODEC.fieldOf("root_provider").forGetter(c -> c.rootProvider),
            AboveRootPlacement.CODEC.optionalFieldOf("above_root_placement").forGetter(c -> c.aboveRootPlacement));
    }
    public RootPlacer(IntProvider trunkOffsetY, Holder<BlockStateProvider> rootProvider, Optional<AboveRootPlacement> aboveRootPlacement) {
        this.trunkOffsetY = trunkOffsetY;
        this.rootProvider = rootProvider;
        this.aboveRootPlacement = aboveRootPlacement;
    }
    protected abstract RootPlacerType<?> type();
    public abstract boolean placeRoots(WorldGenLevel level, BiConsumer<BlockPos, BlockState> rootSetter, RandomSource random, BlockPos origin, BlockPos trunkOrigin, TreeFeature tree);
    protected boolean canPlaceRoot(LevelSimulatedReader level, BlockPos pos) {
        return TreeFeature.validTreePos(level, pos);
    }
    protected void placeRoot(WorldGenLevel level, BiConsumer<BlockPos, BlockState> rootSetter, RandomSource random, BlockPos pos, TreeFeature tree) {
        if (!canPlaceRoot(level, pos)) {
            return;
        }
        rootSetter.accept(pos, getPotentiallyWaterloggedState(level, pos, rootProvider.value().getState(level, random, pos)));
        if (aboveRootPlacement.isPresent()) {
            AboveRootPlacement above = aboveRootPlacement.get();
            BlockPos top = pos.above();
            if (random.nextFloat() < above.aboveRootPlacementChance() && level.isStateAtPosition(top, BlockState::isAir)) {
                rootSetter.accept(top, getPotentiallyWaterloggedState(level, top, above.aboveRootProvider().value().getState(level, random, top)));
            }
        }
    }
    protected BlockState getPotentiallyWaterloggedState(LevelSimulatedReader level, BlockPos pos, BlockState state) {
        if (!state.hasProperty(net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED)) {
            return state;
        }
        boolean water = level.isFluidAtPosition(pos, fluid -> fluid.is(net.minecraft.tags.FluidTags.WATER));
        return state.setValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED, water);
    }
    public BlockPos getTrunkOrigin(BlockPos origin, RandomSource random) {
        return origin.above(trunkOffsetY.sample(random));
    }
}
