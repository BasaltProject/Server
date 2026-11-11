// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
public record CountPlacement(IntProvider count) implements RepeatingPlacement {
    public static final MapCodec<CountPlacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.placement.CountPlacement.class, "CODEC");
    public static CountPlacement of(IntProvider count) {
        return new CountPlacement(count);
    }
    public static CountPlacement of(int count) {
        return of(net.minecraft.util.valueproviders.ConstantInt.of(count));
    }
    public int count(RandomSource random, BlockPos origin) {
        return count.sample(random);
    }
    public MapCodec<CountPlacement> codec() {
        return CODEC;
    }
}
