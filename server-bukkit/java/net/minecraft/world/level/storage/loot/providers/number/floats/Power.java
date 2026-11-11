// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.PowerProvider;
public record Power(Holder<ContextFloatProvider> base, Holder<ContextFloatProvider> exponent) implements ContextFloatProvider, PowerProvider<ContextFloatProvider> {
    public static final MapCodec<Power> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.Power.class, "MAP_CODEC");
    public MapCodec<Power> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        float b = base.value().getFloatUnsafe(context);
        float e = exponent.value().getFloatUnsafe(context);
        return b == 0.0F && e == 0.0F ? Float.NaN : (float) Math.pow((double) b, (double) e);

    }
}
