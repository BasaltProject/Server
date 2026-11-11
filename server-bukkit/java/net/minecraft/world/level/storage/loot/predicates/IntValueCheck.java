// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.IntRangePredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public record IntValueCheck(Holder<ContextIntProvider> value, IntRangePredicate range) implements LootItemCondition {
    public static final MapCodec<IntValueCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.IntValueCheck.class, "MAP_CODEC");
    public MapCodec<IntValueCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
    }
    public boolean test(LootContext context) {
        return range.test(context, value.value());
    }
    public static LootItemCondition.Builder hasValue(Holder<ContextIntProvider> value, IntRangePredicate range) {
        return () -> new IntValueCheck(value, range);
    }
}
