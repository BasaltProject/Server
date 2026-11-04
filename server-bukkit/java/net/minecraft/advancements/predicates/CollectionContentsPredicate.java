// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
public interface CollectionContentsPredicate<T, P extends Predicate<T>> extends Predicate<Iterable<? extends T>> {
    List<P> unpack();
    static <T, P extends Predicate<T>> Codec<CollectionContentsPredicate<T, P>> codec(Codec<P> elementCodec) {
        return elementCodec.listOf().xmap(CollectionContentsPredicate::of, CollectionContentsPredicate::unpack);
    }
    @SafeVarargs
    static <T, P extends Predicate<T>> CollectionContentsPredicate<T, P> of(P... predicates) {
        return of(List.of(predicates));
    }
    static <T, P extends Predicate<T>> CollectionContentsPredicate<T, P> of(List<P> predicates) {
        return switch (predicates.size()) {
            case 0 -> new Zero<>();
            case 1 -> new Single<>(predicates.getFirst());
            default -> new Multiple<>(predicates);
        };
    }
    public static class Zero<T, P extends Predicate<T>> implements CollectionContentsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            return true;
        }
        public List<P> unpack() {
            return List.of();
        }
    }
    public static record Single<T, P extends Predicate<T>>(P test) implements CollectionContentsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            for (T value : values) {
                if (test.test(value)) {
                    return true;
                }
            }
            return false;
        }
        public List<P> unpack() {
            return List.of(test);
        }
    }
    public static record Multiple<T, P extends Predicate<T>>(List<P> tests) implements CollectionContentsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            List<Predicate<T>> pending = new ArrayList<>(tests);
            for (T value : values) {
                pending.removeIf(p -> p.test(value));
                if (pending.isEmpty()) {
                    return true;
                }
            }
            return false;
        }
        public List<P> unpack() {
            return tests;
        }
    }
}
