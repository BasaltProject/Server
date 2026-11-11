// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.DispatcherProvider;
public record NumberDispatcher(List<DispatcherProvider.Case<ContextIntProvider>> cases, Holder<ContextIntProvider> defaultValue) implements ContextIntProvider, DispatcherProvider<ContextIntProvider> {
    public static final MapCodec<NumberDispatcher> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.NumberDispatcher.class, "MAP_CODEC");
    public int getIntUnsafe(LootContext context) {
        return selectValue(context).value().getIntUnsafe(context);

    }
    public MapCodec<NumberDispatcher> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
