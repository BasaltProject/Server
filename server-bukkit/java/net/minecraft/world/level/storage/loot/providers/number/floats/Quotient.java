// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.BinaryProvider;
public record Quotient(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) implements ContextFloatProvider, BinaryProvider<ContextFloatProvider> {
    public static final MapCodec<Quotient> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.Quotient.class, "MAP_CODEC");
    public MapCodec<Quotient> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        return left().value().getFloatUnsafe(context) / right().value().getFloatUnsafe(context);

    }
}
