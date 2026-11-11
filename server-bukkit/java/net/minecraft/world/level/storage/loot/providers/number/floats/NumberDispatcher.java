// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.DispatcherProvider;
public record NumberDispatcher(List<DispatcherProvider.Case<ContextFloatProvider>> cases, Holder<ContextFloatProvider> defaultValue) implements ContextFloatProvider, DispatcherProvider<ContextFloatProvider> {
    public static final MapCodec<NumberDispatcher> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.NumberDispatcher.class, "MAP_CODEC");
    public float getFloatUnsafe(LootContext context) {
        return selectValue(context).value().getFloatUnsafe(context);

    }
    public MapCodec<NumberDispatcher> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
