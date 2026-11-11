// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public record RandomlySelectedPlacement(List<PlacementModifier> placements) implements PlacementModifier {
    public static final MapCodec<RandomlySelectedPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.RandomlySelectedPlacement.class, "CODEC");
    public RandomlySelectedPlacement(PlacementModifier... placements) {
        this(List.of(placements));
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        placements.get(random.nextInt(placements.size())).modify(context, random, origin, output);
    }
    public MapCodec<RandomlySelectedPlacement> codec() {
        return CODEC;
    }
}
