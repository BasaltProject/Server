// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.advancements.predicates.DamageSourcePredicate;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public record DamageSourceCondition(Optional<DamageSourcePredicate> predicate) implements LootItemCondition {
    public static final MapCodec<DamageSourceCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.DamageSourceCondition.class, "MAP_CODEC");
    public MapCodec<DamageSourceCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ORIGIN, LootContextParams.DAMAGE_SOURCE);
    }
    public boolean test(LootContext context) {
        var source = context.getOptional(LootContextParams.DAMAGE_SOURCE);
        var origin = context.getOptional(LootContextParams.ORIGIN);
        if (source == null || origin == null) {
            return false;
        }
        return predicate.isEmpty() || predicate.get().matches(context.getLevel(), origin, source);
    }
    public static LootItemCondition.Builder hasDamageSource(DamageSourcePredicate.Builder builder) {
        return () -> new DamageSourceCondition(Optional.of(builder.build()));
    }
}
