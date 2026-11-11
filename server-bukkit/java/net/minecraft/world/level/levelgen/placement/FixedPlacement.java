// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record FixedPlacement(List<BlockPos> positions) implements PlacementModifier {
    public static final MapCodec<FixedPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.FixedPlacement.class, "CODEC");
    public static FixedPlacement of(BlockPos... pos) {
        return new FixedPlacement(List.of(pos));
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int chunkX = origin.getX() >> 4;
        int chunkZ = origin.getZ() >> 4;
        for (BlockPos fixed : positions) {
            if (fixed.getX() >> 4 == chunkX && fixed.getZ() >> 4 == chunkZ) {
                output.accept(fixed);
            }
        }
    }
    public MapCodec<FixedPlacement> codec() {
        return CODEC;
    }
}
