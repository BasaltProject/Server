// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;

import com.mojang.serialization.MapCodec;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.TrapezoidInt;

public record OffsetPlacement(IntProvider x, IntProvider y, IntProvider z) implements PlacementModifier {
    public static final MapCodec<OffsetPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.OffsetPlacement.class, "CODEC");

    public static OffsetPlacement of(IntProvider xzSpread, IntProvider ySpread) {
        return new OffsetPlacement(xzSpread, ySpread, xzSpread);
    }

    public static OffsetPlacement ofTriangle(int xzRange, int yRange) {
        return of(TrapezoidInt.triangle(xzRange), TrapezoidInt.triangle(yRange));
    }

    public static OffsetPlacement vertical(IntProvider ySpread) {
        return of(ConstantInt.of(0), ySpread);
    }

    public static OffsetPlacement horizontal(IntProvider xzSpread) {
        return of(xzSpread, ConstantInt.of(0));
    }

    public static OffsetPlacement of(int x, int y, int z) {
        return new OffsetPlacement(ConstantInt.of(x), ConstantInt.of(y), ConstantInt.of(z));
    }

    public static OffsetPlacement of(Direction direction) {
        return of(direction.getStepX(), direction.getStepY(), direction.getStepZ());
    }

    public static OffsetPlacement above() {
        return of(0, 1, 0);
    }

    public void modify(PlacementContext context, RandomSource random, BlockPos origin, Consumer<BlockPos> output) {
        int dx = x.sample(random);
        int dy = y.sample(random);
        int dz = z.sample(random);
        output.accept(origin.offset(dx, dy, dz));
    }

    public MapCodec<OffsetPlacement> codec() {
        return CODEC;
    }
}
