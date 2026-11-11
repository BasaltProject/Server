// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.FloatRangePredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public record FloatValueCheck(Holder<ContextFloatProvider> value, FloatRangePredicate range) implements LootItemCondition {
    public static final MapCodec<FloatValueCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.FloatValueCheck.class, "MAP_CODEC");
    public MapCodec<FloatValueCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
    }
    public boolean test(LootContext context) {
        return range.test(context, value.value());
    }
    public static LootItemCondition.Builder hasValue(Holder<ContextFloatProvider> value, FloatRangePredicate range) {
        return () -> new FloatValueCheck(value, range);
    }
}
