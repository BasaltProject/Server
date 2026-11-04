// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.datafixers.util.Either;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;
import java.util.Spliterator;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;

public interface HolderSet<T> extends Iterable<Holder<T>> {
    int size();

    boolean contains(Holder<T> holder);

    Stream<Holder<T>> stream();

    boolean isBound();

    Either<TagKey<T>, List<Holder<T>>> unwrap();

    Optional<Holder<T>> getRandomElement(RandomSource random);

    Holder<T> get(int index);

    boolean canSerializeIn(HolderOwner<T> owner);

    Optional<TagKey<T>> unwrapKey();

    static <T> Named<T> emptyNamed(HolderOwner<T> owner, TagKey<T> key) {
        return new Named<>(owner, key);
    }

    @SuppressWarnings("unchecked")
    static <T> HolderSet<T> empty() {
        return (HolderSet<T>) Direct.EMPTY;
    }

    @SafeVarargs
    static <T> Direct<T> direct(Holder<T>... values) {
        return new Direct<>(List.of(values));
    }

    static <T> Direct<T> direct(List<? extends Holder<T>> values) {
        return new Direct<>(List.copyOf(values));
    }

    @SafeVarargs
    static <E, T> Direct<T> direct(Function<E, Holder<T>> holderGetter, E... elements) {
        return direct(Stream.of(elements).map(holderGetter).toList());
    }

    static <E, T> Direct<T> direct(Function<E, Holder<T>> holderGetter, Collection<E> elements) {
        return direct(elements.stream().map(holderGetter).toList());
    }

    abstract class ListBacked<T> implements HolderSet<T> {
        protected abstract List<Holder<T>> contents();

        @Override
        public int size() {
            return contents().size();
        }

        @Override
        public Iterator<Holder<T>> iterator() {
            return contents().iterator();
        }

        @Override
        public boolean contains(Holder<T> holder) {
            return contents().contains(holder);
        }

        @Override
        public Spliterator<Holder<T>> spliterator() {
            return contents().spliterator();
        }

        @Override
        public Stream<Holder<T>> stream() {
            return contents().stream();
        }

        @Override
        public Optional<Holder<T>> getRandomElement(RandomSource random) {
            List<Holder<T>> contents = contents();
            return contents.isEmpty() ? Optional.empty() : Optional.of(contents.get(random.nextInt(contents.size())));
        }

        @Override
        public Holder<T> get(int index) {
            return contents().get(index);
        }

        @Override
        public boolean canSerializeIn(HolderOwner<T> owner) {
            return true;
        }
    }

    class Named<T> extends ListBacked<T> {
        private final HolderOwner<T> owner;
        private final TagKey<T> key;
        private List<Holder<T>> contents = List.of();
        private boolean bound;

        public Named(TagKey<T> key) {
            this(null, key);
        }

        Named(HolderOwner<T> owner, TagKey<T> key) {
            this.owner = owner;
            this.key = key;
        }

        public TagKey<T> key() {
            return key;
        }

        public void bind(List<Holder<T>> holders) {
            contents = List.copyOf(holders);
            bound = true;
        }

        @Override
        protected List<Holder<T>> contents() {
            return contents;
        }

        @Override
        public boolean isBound() {
            return bound;
        }

        @Override
        public Either<TagKey<T>, List<Holder<T>>> unwrap() {
            return Either.left(key);
        }

        @Override
        public Optional<TagKey<T>> unwrapKey() {
            return Optional.of(key);
        }

        @Override
        public boolean contains(Holder<T> value) {
            return bound ? super.contains(value) : value.is(key);
        }

        @Override
        public String toString() {
            return "NamedSet(" + key + ")" + contents;
        }

        @Override
        public boolean canSerializeIn(HolderOwner<T> context) {
            return owner == null || owner.canSerializeIn(context);
        }
    }

    final class Direct<T> extends ListBacked<T> {
        static final Direct<?> EMPTY = new Direct<>(List.of());
        private final List<Holder<T>> contents;
        private java.util.Set<Holder<T>> contentsSet;

        private Direct(List<Holder<T>> contents) {
            this.contents = contents;
        }

        @Override
        protected List<Holder<T>> contents() {
            return contents;
        }

        @Override
        public boolean isBound() {
            return true;
        }

        @Override
        public Either<TagKey<T>, List<Holder<T>>> unwrap() {
            return Either.right(contents);
        }

        @Override
        public Optional<TagKey<T>> unwrapKey() {
            return Optional.empty();
        }

        @Override
        public boolean contains(Holder<T> value) {
            if (contentsSet == null) {
                contentsSet = java.util.Set.copyOf(contents);
            }
            return contentsSet.contains(value);
        }

        @Override
        public String toString() {
            return "DirectSet" + contents;
        }

        @Override
        public boolean equals(Object obj) {
            return this == obj || obj instanceof Direct<?> other && contents.equals(other.contents);
        }

        @Override
        public int hashCode() {
            return contents.hashCode();
        }
    }
}
