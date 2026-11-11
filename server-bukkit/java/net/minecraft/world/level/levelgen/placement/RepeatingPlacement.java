// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public interface RepeatingPlacement extends PlacementModifier {
    int count(RandomSource random, BlockPos origin);
    default void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int copies = count(random, origin);
        for (int at = 0; at < copies; at++) {
            output.accept(origin);
        }
    }
    MapCodec<? extends RepeatingPlacement> codec();
}
