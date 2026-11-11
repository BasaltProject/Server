// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.UnaryProvider;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public record FromFloat(Holder<ContextFloatProvider> input) implements ContextIntProvider, UnaryProvider<ContextFloatProvider> {
    public static final MapCodec<FromFloat> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.FromFloat.class, "MAP_CODEC");
    public MapCodec<FromFloat> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) {
        return ContextIntProvider.floatToIntSafe(input().value().getFloatUnsafe(context));

    }
}
