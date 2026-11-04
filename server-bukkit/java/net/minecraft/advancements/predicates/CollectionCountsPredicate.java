// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.function.Predicate;
public interface CollectionCountsPredicate<T, P extends Predicate<T>> extends Predicate<Iterable<? extends T>> {
    List<Entry<T, P>> unpack();
    static <T, P extends Predicate<T>> Codec<CollectionCountsPredicate<T, P>> codec(Codec<P> elementCodec) {
        return Entry.codec(elementCodec).listOf().xmap(CollectionCountsPredicate::of, CollectionCountsPredicate::unpack);
    }
    @SafeVarargs
    static <T, P extends Predicate<T>> CollectionCountsPredicate<T, P> of(Entry<T, P>... predicates) {
        return of(List.of(predicates));
    }
    static <T, P extends Predicate<T>> CollectionCountsPredicate<T, P> of(List<Entry<T, P>> predicates) {
        return switch (predicates.size()) {
            case 0 -> new Zero<>();
            case 1 -> new Single<>(predicates.getFirst());
            default -> new Multiple<>(predicates);
        };
    }
    public static class Zero<T, P extends Predicate<T>> implements CollectionCountsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            return true;
        }
        public List<Entry<T, P>> unpack() {
            return List.of();
        }
    }
    public static record Single<T, P extends Predicate<T>>(Entry<T, P> entry) implements CollectionCountsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            return entry.test(values);
        }
        public List<Entry<T, P>> unpack() {
            return List.of(entry);
        }
    }
    public static record Multiple<T, P extends Predicate<T>>(List<Entry<T, P>> entries) implements CollectionCountsPredicate<T, P> {
        public boolean test(Iterable<? extends T> values) {
            for (Entry<T, P> entry : entries) {
                if (!entry.test(values)) {
                    return false;
                }
            }
            return true;
        }
        public List<Entry<T, P>> unpack() {
            return entries;
        }
    }
    public static record Entry<T, P extends Predicate<T>>(P test, MinMaxBounds.Ints count) {
        public static <T, P extends Predicate<T>> Codec<Entry<T, P>> codec(Codec<P> elementCodec) {
            return RecordCodecBuilder.create(i -> i.group(elementCodec.fieldOf("test").forGetter(Entry::test),
                MinMaxBounds.Ints.CODEC.fieldOf("count").forGetter(Entry::count)).apply(i, Entry::new));
        }
        public boolean test(Iterable<? extends T> values) {
            int matched = 0;
            for (T value : values) {
                if (test.test(value)) {
                    matched++;
                }
            }
            return count.matches(matched);
        }
    }
}
