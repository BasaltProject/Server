// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.AggregateProvider;
public record Average(HolderSet<ContextIntProvider> inputs) implements ContextIntProvider, AggregateProvider<ContextIntProvider> {
    public static final MapCodec<Average> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.Average.class, "MAP_CODEC");
    public MapCodec<Average> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) throws ArithmeticException {
        long total = 0L;
        long n = 0L;
        for (var each : inputs()) {
            total += each.value().getIntUnsafe(context);
            n++;
        }
        return ContextIntProvider.longToIntSafe(total / n);

    }
}
