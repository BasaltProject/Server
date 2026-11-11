// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import net.minecraft.world.level.WorldGenLevel;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
public record EnvironmentScanPlacement(Direction directionOfSearch, BlockPredicate targetCondition, BlockPredicate allowedSearchCondition, int maxSteps) implements PlacementModifier {
    public static final MapCodec<EnvironmentScanPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.EnvironmentScanPlacement.class, "CODEC");
    public static EnvironmentScanPlacement scanningFor(Direction directionOfSearch, BlockPredicate targetCondition, BlockPredicate allowedSearchCondition, int maxSteps) {
        return new EnvironmentScanPlacement(directionOfSearch, targetCondition, allowedSearchCondition, maxSteps);
    }
    public static EnvironmentScanPlacement scanningFor(Direction directionOfSearch, BlockPredicate targetCondition, int maxSteps) {
        return scanningFor(directionOfSearch, targetCondition, net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate.alwaysTrue(), maxSteps);
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        BlockPos.MutableBlockPos cursor = origin.mutable();
        WorldGenLevel level = context.getLevel();
        if (!allowedSearchCondition.test(level, cursor)) {
            return;
        }
        int steps = 0;
        while (steps < maxSteps) {
            if (targetCondition.test(level, cursor)) {
                output.accept(cursor);
                return;
            }
            cursor.move(directionOfSearch);
            if (level.isOutsideBuildHeight(cursor.getY())) {
                return;
            }
            if (!allowedSearchCondition.test(level, cursor)) {
                break;
            }
            steps++;
        }
        if (targetCondition.test(level, cursor)) {
            output.accept(cursor);
        }
    }
    public MapCodec<EnvironmentScanPlacement> codec() {
        return CODEC;
    }
}
