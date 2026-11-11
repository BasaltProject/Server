// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
public class InSquarePlacement implements PlacementModifier {
    private static final InSquarePlacement INSTANCE = new InSquarePlacement();
    public static final MapCodec<InSquarePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.InSquarePlacement.class, "CODEC");
    public static InSquarePlacement spread() {
        return INSTANCE;
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        output.accept(new BlockPos(random.nextInt(16) + origin.getX(), origin.getY(), random.nextInt(16) + origin.getZ()));
    }
    public MapCodec<InSquarePlacement> codec() {
        return CODEC;
    }
}
