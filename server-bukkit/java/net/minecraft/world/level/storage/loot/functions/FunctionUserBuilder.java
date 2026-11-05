// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.Holder;
public interface FunctionUserBuilder<T extends FunctionUserBuilder<T>> {
    default T apply(LootItemFunction.Builder builder) {
        return apply(Holder.direct(builder.build()));
    }
    T apply(Holder<LootItemFunction> function);
    default <E> T apply(Iterable<E> collection, Function<E, LootItemFunction.Builder> functionProvider) {
        for (E element : collection) {
            apply(functionProvider.apply(element));
        }
        return unwrap();
    }
    default <E> T apply(E[] collection, Function<E, LootItemFunction.Builder> functionProvider) {
        for (E element : collection) {
            apply(functionProvider.apply(element));
        }
        return unwrap();
    }
    T unwrap();
    static Optional<Holder<LootItemFunction>> buildFunction(List<Holder<LootItemFunction>> conditions) {
        if (conditions.isEmpty()) {
            return Optional.empty();
        }
        if (conditions.size() == 1) {
            return Optional.of(conditions.get(0));
        }
        return Optional.of(Holder.direct(new org.basaltmc.bukkit.loot.FunctionList(conditions)));
    }
}
