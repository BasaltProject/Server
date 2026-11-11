// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.AggregateProvider;
public record Average(HolderSet<ContextFloatProvider> inputs) implements ContextFloatProvider, AggregateProvider<ContextFloatProvider> {
    public static final MapCodec<Average> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.Average.class, "MAP_CODEC");
    public MapCodec<Average> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        float total = 0.0F;
        int n = 0;
        for (var each : inputs()) {
            total += each.value().getFloatUnsafe(context);
            n++;
        }
        return total / n;

    }
}
