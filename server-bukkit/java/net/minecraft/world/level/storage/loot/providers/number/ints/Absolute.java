// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.UnaryProvider;
public record Absolute(Holder<ContextIntProvider> input) implements ContextIntProvider, UnaryProvider<ContextIntProvider> {
    public static final MapCodec<Absolute> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.Absolute.class, "MAP_CODEC");
    public MapCodec<Absolute> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) {
        return Math.absExact(input().value().getIntUnsafe(context));

    }
}
