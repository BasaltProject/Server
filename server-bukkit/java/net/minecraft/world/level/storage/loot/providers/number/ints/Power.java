// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.PowerProvider;
public record Power(Holder<ContextIntProvider> base, Holder<ContextIntProvider> exponent) implements ContextIntProvider, PowerProvider<ContextIntProvider> {
    public static final MapCodec<Power> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.Power.class, "MAP_CODEC");
    public MapCodec<Power> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) throws ArithmeticException {
        int b = base.value().getIntUnsafe(context);
        int e = exponent.value().getIntUnsafe(context);
        if (b == 0 && e == 0) {
            throw new ArithmeticException("Result of 0 to the power of 0 is undefined");
        }
        return Math.powExact(b, e);
    }
}
