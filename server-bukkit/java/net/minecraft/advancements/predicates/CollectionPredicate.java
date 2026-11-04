// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.google.common.collect.Iterables;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
public record CollectionPredicate<T, P extends Predicate<T>>(Optional<CollectionContentsPredicate<T, P>> contains, Optional<CollectionCountsPredicate<T, P>> counts, Optional<MinMaxBounds.Ints> size) implements Predicate<Iterable<? extends T>> {
    public static <T, P extends Predicate<T>> Codec<CollectionPredicate<T, P>> codec(Codec<P> elementCodec) {
        return RecordCodecBuilder.create(i -> i.group(
            CollectionContentsPredicate.<T, P>codec(elementCodec).optionalFieldOf("contains").forGetter(CollectionPredicate::contains),
            CollectionCountsPredicate.<T, P>codec(elementCodec).optionalFieldOf("count").forGetter(CollectionPredicate::counts),
            MinMaxBounds.Ints.CODEC.optionalFieldOf("size").forGetter(CollectionPredicate::size)).apply(i, CollectionPredicate::new));
    }
    public boolean test(Iterable<? extends T> value) {
        if (contains.isPresent() && !contains.get().test(value)) {
            return false;
        }
        if (counts.isPresent() && !counts.get().test(value)) {
            return false;
        }
        return size.isEmpty() || size.get().matches(Iterables.size(value));
    }
}
