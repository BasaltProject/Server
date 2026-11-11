// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.core.Direction;
import com.mojang.serialization.Codec;
import com.mojang.datafixers.util.Function3;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockState;
public abstract class AbstractOreFeature implements Feature {
    private static boolean skipAirCheck(RandomSource random, float chance) {
        if (chance <= 0.0F) {
            return true;
        }
        if (chance >= 1.0F) {
            return false;
        }
        return random.nextFloat() >= chance;
    }
    protected final List<BlockReplacement> targetStates;
    protected final int size;
    protected final float discardChanceOnAirExposure;
    public AbstractOreFeature(List<BlockReplacement> targetStates, int size, float discardChanceOnAirExposure) {
        this.targetStates = targetStates;
        this.size = size;
        this.discardChanceOnAirExposure = discardChanceOnAirExposure;
    }
    protected static <T extends AbstractOreFeature> MapCodec<T> makeCodec(Function3<List<BlockReplacement>, Integer, Float, T> constructor) {
        return com.mojang.serialization.codecs.RecordCodecBuilder.<T>mapCodec(i -> i.group(
            Codec.list(BlockReplacement.CODEC).fieldOf("targets").forGetter(AbstractOreFeature::targetStates),
            Codec.intRange(0, 64).fieldOf("size").forGetter(AbstractOreFeature::size),
            Codec.floatRange(0.0F, 1.0F).fieldOf("discard_chance_on_air_exposure").forGetter(AbstractOreFeature::discardChanceOnAirExposure))
            .apply(i, constructor));
    }
    public abstract MapCodec<? extends AbstractOreFeature> codec();
    public final List<BlockReplacement> targetStates() {
        return targetStates;
    }
    public final int size() {
        return size;
    }
    public final float discardChanceOnAirExposure() {
        return discardChanceOnAirExposure;
    }
    public boolean canPlaceOre(BlockState state, Function<BlockPos, BlockState> blockGetter, RandomSource random, BlockReplacement targetState, BlockPos.MutableBlockPos orePos) {
        if (!targetState.target().test(state, orePos, random)) {
            return false;
        }
        return skipAirCheck(random, discardChanceOnAirExposure) || !isAdjacentToAir(blockGetter, orePos);
    }
    public static boolean isAdjacentToAir(Function<BlockPos, BlockState> blockGetter, BlockPos pos) {
        return checkNeighbors(blockGetter, pos, BlockState::isAir);
    }
    public static boolean checkNeighbors(Function<BlockPos, BlockState> blockGetter, BlockPos pos, Predicate<BlockState> predicate) {
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        for (Direction side : Direction.values()) {
            probe.setWithOffset(pos, side);
            if (predicate.test(blockGetter.apply(probe))) {
                return true;
            }
        }
        return false;
    }
}
