// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ConditionalProvider;
public record ConditionalValue(Holder<LootItemCondition> condition, Holder<ContextIntProvider> onTrue, Holder<ContextIntProvider> onFalse) implements ContextIntProvider, ConditionalProvider<ContextIntProvider> {
    public static final MapCodec<ConditionalValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.ConditionalValue.class, "MAP_CODEC");
    public static final Codec<ConditionalValue> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ConditionalValue.class, "CODEC");
    public int getIntUnsafe(LootContext context) {
        return selectValue(context).value().getIntUnsafe(context);

    }
    public MapCodec<ConditionalValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
