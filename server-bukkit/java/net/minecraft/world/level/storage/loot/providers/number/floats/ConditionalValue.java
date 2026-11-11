// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ConditionalProvider;
public record ConditionalValue(Holder<LootItemCondition> condition, Holder<ContextFloatProvider> onTrue, Holder<ContextFloatProvider> onFalse) implements ContextFloatProvider, ConditionalProvider<ContextFloatProvider> {
    public static final MapCodec<ConditionalValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.ConditionalValue.class, "MAP_CODEC");
    public static final Codec<ConditionalValue> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ConditionalValue.class, "CODEC");
    public float getFloatUnsafe(LootContext context) {
        return selectValue(context).value().getFloatUnsafe(context);

    }
    public MapCodec<ConditionalValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
