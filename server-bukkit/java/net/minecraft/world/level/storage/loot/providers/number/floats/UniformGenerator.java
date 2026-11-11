// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.RangeProvider;
public record UniformGenerator(Holder<ContextFloatProvider> min, Holder<ContextFloatProvider> max) implements ContextFloatProvider, RangeProvider<ContextFloatProvider> {
    public static final MapCodec<UniformGenerator> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.UniformGenerator.class, "MAP_CODEC");
    public MapCodec<UniformGenerator> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        return Mth.nextFloat(context.getRandom(), min().value().getFloatUnsafe(context), max().value().getFloatUnsafe(context));

    }
}
