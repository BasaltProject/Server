// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.AggregateProvider;
public record Minimum(HolderSet<ContextIntProvider> inputs) implements ContextIntProvider, AggregateProvider<ContextIntProvider> {
    public static final MapCodec<Minimum> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.Minimum.class, "MAP_CODEC");
    public MapCodec<Minimum> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) {
        int acc = Integer.MAX_VALUE;
        for (var each : inputs()) {
            acc = Math.min(acc, each.value().getIntUnsafe(context));
        }
        return acc;

    }
}
