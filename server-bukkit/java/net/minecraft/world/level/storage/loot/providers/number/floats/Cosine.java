// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.UnaryProvider;
public record Cosine(Holder<ContextFloatProvider> input) implements ContextFloatProvider, UnaryProvider<ContextFloatProvider> {
    public static final MapCodec<Cosine> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.Cosine.class, "MAP_CODEC");
    public MapCodec<Cosine> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        float v = input().value().getFloatUnsafe(context);
        return Mth.cos((double) v);

    }
}
