// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public interface PlacementFilter extends PlacementModifier {
    default void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        if (shouldPlace(context, random, origin)) {
            output.accept(origin);
        }
    }
    boolean shouldPlace(PlacementContext context, RandomSource random, BlockPos origin);
    MapCodec<? extends PlacementFilter> codec();
}
