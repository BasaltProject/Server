// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.DistributionProvider;
public record WeightedListValue(WeightedList<Holder<ContextFloatProvider>> distribution) implements ContextFloatProvider, DistributionProvider<ContextFloatProvider> {
    public static final MapCodec<WeightedListValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.WeightedListValue.class, "MAP_CODEC");
    public float getFloatUnsafe(LootContext context) {
        return distribution().getRandomOrThrow(context.getRandom()).value().getFloatUnsafe(context);

    }
    public MapCodec<WeightedListValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
