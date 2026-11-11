// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.rootplacers;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.LevelSimulatedReader;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.feature.TreeFeature;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public class MangroveRootPlacer extends RootPlacer {
    private final MangroveRootPlacement mangroveRootPlacement;
    private boolean simulateRoots(LevelSimulatedReader level, RandomSource random, BlockPos rootPos, Direction dir, BlockPos rootOrigin, List<BlockPos> found, int layer) {
        int maxLength = mangroveRootPlacement.maxRootLength();
        if (layer == maxLength || found.size() > maxLength) {
            return false;
        }
        for (BlockPos next : potentialRootPositions(rootPos, dir, random, rootOrigin)) {
            if (canPlaceRoot(level, next)) {
                found.add(next);
                if (!simulateRoots(level, random, next, dir, rootOrigin, found, layer + 1)) {
                    return false;
                }
            }
        }
        return true;
    }
    public static final int ROOT_WIDTH_LIMIT = 8;
    public static final int ROOT_LENGTH_LIMIT = 15;
    public static final MapCodec<MangroveRootPlacer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.rootplacers.MangroveRootPlacer.class, "CODEC");
    public MangroveRootPlacer(IntProvider trunkOffsetY, Holder<BlockStateProvider> rootProvider, Optional<AboveRootPlacement> aboveRootPlacement, MangroveRootPlacement mangroveRootPlacement) {
        super(trunkOffsetY, rootProvider, aboveRootPlacement);
        this.mangroveRootPlacement = mangroveRootPlacement;
    }
    public boolean placeRoots(WorldGenLevel level, BiConsumer<BlockPos, BlockState> rootSetter, RandomSource random, BlockPos origin, BlockPos trunkOrigin, TreeFeature tree) {
        List<BlockPos> roots = new java.util.ArrayList<>();
        BlockPos.MutableBlockPos column = origin.mutable();
        while (column.getY() < trunkOrigin.getY()) {
            if (!canPlaceRoot(level, column)) {
                return false;
            }
            column.move(Direction.UP);
        }
        roots.add(trunkOrigin.below());
        for (Direction side : Direction.Plane.HORIZONTAL) {
            BlockPos start = trunkOrigin.relative(side);
            List<BlockPos> branch = new java.util.ArrayList<>();
            if (!simulateRoots(level, random, start, side, trunkOrigin, branch, 0)) {
                return false;
            }
            roots.addAll(branch);
            roots.add(start);
        }
        for (BlockPos root : roots) {
            placeRoot(level, rootSetter, random, root, tree);
        }
        return true;
    }
    protected List<BlockPos> potentialRootPositions(BlockPos pos, Direction prevDir, RandomSource random, BlockPos rootOrigin) {
        BlockPos below = pos.below();
        BlockPos beside = pos.relative(prevDir);
        int width = pos.distManhattan(rootOrigin);
        int maxWidth = mangroveRootPlacement.maxRootWidth();
        float skew = mangroveRootPlacement.randomSkewChance();
        if (width > maxWidth - 3 && width <= maxWidth) {
            return random.nextFloat() < skew ? List.of(below, beside.below()) : List.of(below);
        }
        if (width > maxWidth || random.nextFloat() < skew) {
            return List.of(below);
        }
        return random.nextBoolean() ? List.of(beside) : List.of(below);
    }
    protected boolean canPlaceRoot(LevelSimulatedReader level, BlockPos pos) {
        return super.canPlaceRoot(level, pos) || level.isStateAtPosition(pos, state -> state.is(mangroveRootPlacement.canGrowThrough()));
    }
    protected void placeRoot(WorldGenLevel level, BiConsumer<BlockPos, BlockState> rootSetter, RandomSource random, BlockPos pos, TreeFeature tree) {
        if (level.isStateAtPosition(pos, state -> state.is(mangroveRootPlacement.muddyRootsIn()))) {
            BlockState muddy = mangroveRootPlacement.muddyRootsProvider().value().getState(level, random, pos);
            rootSetter.accept(pos, getPotentiallyWaterloggedState(level, pos, muddy));
        } else {
            super.placeRoot(level, rootSetter, random, pos, tree);
        }
    }
    protected RootPlacerType<?> type() {
        return RootPlacerType.MANGROVE_ROOT_PLACER;


    }
}
