// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.UnaryProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public record FromInt(Holder<ContextIntProvider> input) implements ContextFloatProvider, UnaryProvider<ContextIntProvider> {
    public static final MapCodec<FromInt> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.FromInt.class, "MAP_CODEC");
    public MapCodec<FromInt> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        return ContextFloatProvider.intToFloatSafe(input().value().getIntUnsafe(context));

    }
}
