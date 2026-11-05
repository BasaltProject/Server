// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.Holder;
public interface ConditionUserBuilder<T extends ConditionUserBuilder<T>> {
    default T when(LootItemCondition.Builder builder) {
        return when(Holder.direct(builder.build()));
    }
    T when(Holder<LootItemCondition> condition);
    default <E> T when(Iterable<E> collection, Function<E, LootItemCondition.Builder> conditionProvider) {
        for (E element : collection) {
            when(conditionProvider.apply(element));
        }
        return unwrap();
    }
    T unwrap();
    static Optional<Holder<LootItemCondition>> buildCondition(List<Holder<LootItemCondition>> conditions) {
        if (conditions.isEmpty()) {
            return Optional.empty();
        }
        if (conditions.size() == 1) {
            return Optional.of(conditions.get(0));
        }
        return Optional.of(Holder.direct(new org.basaltmc.bukkit.loot.ConditionList(conditions)));
    }
}
