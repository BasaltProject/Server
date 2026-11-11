// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;

public class MultifaceSpreader {
    public static SpreadType[] DEFAULT_SPREAD_ORDER = {SpreadType.SAME_POSITION, SpreadType.SAME_PLANE, SpreadType.WRAP_AROUND};

    private final SpreadConfig config;

    public MultifaceSpreader(MultifaceBlock multifaceBlock) {
        this(new DefaultSpreaderConfig(multifaceBlock));
    }

    public MultifaceSpreader(SpreadConfig config) {
        this.config = config;
    }

    public boolean canSpreadInAnyDirection(BlockState state, BlockGetter level, BlockPos pos, Direction startingFace) {
        for (Direction toward : Direction.values()) {
            if (getSpreadFromFaceTowardDirection(state, level, pos, startingFace, toward, config::canSpreadInto).isPresent()) {
                return true;
            }
        }
        return false;
    }

    public Optional<SpreadPos> spreadFromRandomFaceTowardRandomDirection(BlockState state, LevelAccessor level, BlockPos pos, RandomSource random) {
        for (Direction face : Direction.allShuffled(random)) {
            if (!config.canSpreadFrom(state, face)) {
                continue;
            }
            Optional<SpreadPos> done = spreadFromFaceTowardRandomDirection(state, level, pos, face, random, false);
            if (done.isPresent()) {
                return done;
            }
        }
        return Optional.empty();
    }

    public long spreadAll(BlockState state, LevelAccessor level, BlockPos pos, boolean postProcess) {
        long placed = 0;
        for (Direction face : Direction.values()) {
            if (!config.canSpreadFrom(state, face)) {
                continue;
            }
            for (Direction toward : Direction.values()) {
                placed += spreadFromFaceTowardDirection(state, level, pos, face, toward, postProcess).isPresent() ? 1 : 0;
            }
        }
        return placed;
    }

    public Optional<SpreadPos> spreadFromFaceTowardRandomDirection(BlockState state, LevelAccessor level, BlockPos pos, Direction startingFace, RandomSource random, boolean postProcess) {
        for (Direction toward : Direction.allShuffled(random)) {
            Optional<SpreadPos> done = spreadFromFaceTowardDirection(state, level, pos, startingFace, toward, postProcess);
            if (done.isPresent()) {
                return done;
            }
        }
        return Optional.empty();
    }

    public Optional<SpreadPos> spreadFromFaceTowardDirection(BlockState state, LevelAccessor level, BlockPos pos, Direction fromFace, Direction spreadDirection, boolean postProcess) {
        Optional<SpreadPos> target = getSpreadFromFaceTowardDirection(state, level, pos, fromFace, spreadDirection, config::canSpreadInto);
        return target.isPresent() ? spreadToFace(level, target.get(), postProcess) : Optional.empty();
    }

    public Optional<SpreadPos> getSpreadFromFaceTowardDirection(BlockState state, BlockGetter level, BlockPos pos, Direction startingFace, Direction spreadDirection, SpreadPredicate canSpreadInto) {
        if (spreadDirection.getAxis() == startingFace.getAxis()) {
            return Optional.empty();
        }
        boolean source = config.isOtherBlockValidAsSource(state) || config.hasFace(state, startingFace) && !config.hasFace(state, spreadDirection);
        if (!source) {
            return Optional.empty();
        }
        for (SpreadType type : config.getSpreadTypes()) {
            SpreadPos candidate = type.getSpreadPos(pos, spreadDirection, startingFace);
            if (canSpreadInto.test(level, pos, candidate)) {
                return Optional.of(candidate);
            }
        }
        return Optional.empty();
    }

    public Optional<SpreadPos> spreadToFace(LevelAccessor level, SpreadPos spreadPos, boolean postProcess) {
        BlockState existing = level.getBlockState(spreadPos.pos());
        return config.placeBlock(level, spreadPos, existing, postProcess) ? Optional.of(spreadPos) : Optional.empty();
    }

    public static record SpreadPos(BlockPos pos, Direction face) {}

    public interface SpreadConfig {
        BlockState getStateForPlacement(BlockState oldState, BlockGetter level, BlockPos placementPos, Direction placementDirection);

        boolean canSpreadInto(BlockGetter level, BlockPos sourcePos, SpreadPos spreadPos);

        default SpreadType[] getSpreadTypes() {
            return DEFAULT_SPREAD_ORDER;
        }

        default boolean hasFace(BlockState state, Direction face) {
            return MultifaceBlock.hasFace(state, face);
        }

        default boolean isOtherBlockValidAsSource(BlockState state) {
            return false;
        }

        default boolean canSpreadFrom(BlockState state, Direction face) {
            return isOtherBlockValidAsSource(state) || hasFace(state, face);
        }

        default boolean placeBlock(LevelAccessor level, SpreadPos spreadPos, BlockState oldState, boolean postProcess) {
            BlockState next = getStateForPlacement(oldState, level, spreadPos.pos(), spreadPos.face());
            if (next == null) {
                return false;
            }
            if (postProcess) {
                level.getChunk(spreadPos.pos()).markPosForPostProcessing(spreadPos.pos());
            }
            return level.setBlock(spreadPos.pos(), next, 2);
        }
    }

    public static class DefaultSpreaderConfig implements SpreadConfig {
        protected MultifaceBlock block;

        public DefaultSpreaderConfig(MultifaceBlock block) {
            this.block = block;
        }

        public BlockState getStateForPlacement(BlockState oldState, BlockGetter level, BlockPos placementPos, Direction placementDirection) {
            return block.getStateForPlacement(oldState, level, placementPos, placementDirection);
        }

        protected boolean stateCanBeReplaced(BlockGetter level, BlockPos sourcePos, BlockPos placementPos, Direction placementDirection, BlockState existingState) {
            return existingState.isAir() || existingState.is(block) || existingState.is(Blocks.WATER) && existingState.getFluidState().isSource();
        }

        public boolean canSpreadInto(BlockGetter level, BlockPos sourcePos, SpreadPos spreadPos) {
            BlockState existing = level.getBlockState(spreadPos.pos());
            return stateCanBeReplaced(level, sourcePos, spreadPos.pos(), spreadPos.face(), existing)
                && block.isValidStateForPlacement(level, existing, spreadPos.pos(), spreadPos.face());
        }
    }

    public static enum SpreadType {
        SAME_POSITION,
        SAME_PLANE,
        WRAP_AROUND;

        public SpreadPos getSpreadPos(BlockPos pos, Direction spreadDirection, Direction fromFace) {
            return switch (this) {
                case SAME_POSITION -> new SpreadPos(pos, spreadDirection);
                case SAME_PLANE -> new SpreadPos(pos.relative(spreadDirection), fromFace);
                case WRAP_AROUND -> new SpreadPos(pos.relative(spreadDirection).relative(fromFace), spreadDirection.getOpposite());
            };
        }
    }

    @FunctionalInterface
    public interface SpreadPredicate {
        boolean test(BlockGetter level, BlockPos sourcePos, SpreadPos spreadPos);
    }
}
