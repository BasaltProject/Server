// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
public record HeightRangePlacement(HeightProvider height) implements PlacementModifier {
    public static final MapCodec<HeightRangePlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.HeightRangePlacement.class, "CODEC");
    public static HeightRangePlacement of(HeightProvider height) {
        return new HeightRangePlacement(height);
    }
    public static HeightRangePlacement uniform(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) {
        return of(net.minecraft.world.level.levelgen.heightproviders.UniformHeight.of(minInclusive, maxInclusive));
    }
    public static HeightRangePlacement triangle(VerticalAnchor minInclusive, VerticalAnchor maxInclusive) {
        return of(net.minecraft.world.level.levelgen.heightproviders.TrapezoidHeight.of(minInclusive, maxInclusive));
    }
    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        output.accept(origin.atY(height.sample(random, context)));
    }
    public MapCodec<HeightRangePlacement> codec() {
        return CODEC;
    }
}
