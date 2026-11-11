// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.BinaryProvider;
public record Modulus(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) implements ContextIntProvider, BinaryProvider<ContextIntProvider> {
    public static final MapCodec<Modulus> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.Modulus.class, "MAP_CODEC");
    public MapCodec<Modulus> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) throws ArithmeticException {
        return left().value().getIntUnsafe(context) % right().value().getIntUnsafe(context);

    }
}
