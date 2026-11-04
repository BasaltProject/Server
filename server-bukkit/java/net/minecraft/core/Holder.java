// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import com.mojang.datafixers.util.Either;
import java.util.Collection;
import java.util.stream.Stream;
import net.minecraft.core.component.DataComponentMap;

public interface Holder<T> {
    T value();

    boolean isBound();

    boolean is(Identifier id);

    boolean is(ResourceKey<T> key);

    boolean is(Predicate<ResourceKey<T>> predicate);

    boolean is(TagKey<T> tag);

    boolean is(Holder<T> other);

    Optional<ResourceKey<T>> unwrapKey();

    static <T> Holder<T> direct(T value) {
        return new Direct<>(value);
    }

    record Direct<T>(T value) implements Holder<T> {
        @Override
        public boolean isBound() {
            return true;
        }

        @Override
        public boolean is(Identifier id) {
            return false;
        }

        @Override
        public boolean is(ResourceKey<T> key) {
            return false;
        }

        @Override
        public boolean is(Predicate<ResourceKey<T>> predicate) {
            return false;
        }

        @Override
        public boolean is(TagKey<T> tag) {
            return false;
        }

        @Override
        public boolean is(Holder<T> other) {
            return other.value() == value;
        }

        @Override
        public Optional<ResourceKey<T>> unwrapKey() {
            return Optional.empty();
        }
            public boolean areComponentsBound() {
            return true;
        }
        public Either<ResourceKey<T>, T> unwrap() {
            return Either.right(value);
        }
        public Kind kind() {
            return Kind.DIRECT;
        }
        public String toString() {
            return "Direct{" + value + "}";
        }
        public boolean canSerializeIn(HolderOwner<T> registry) {
            return true;
        }
        public Stream<TagKey<T>> tags() {
            return Stream.of();
        }
}

    class Reference<T> implements Holder<T> {
        private ResourceKey<T> key;
        private T value;
        private final Registry<T> owner;
        private final HolderOwner<T> holderOwner;
        private final Type type;
        private java.util.Set<TagKey<T>> boundTags = java.util.Set.of();
        private DataComponentMap components;

        public Reference(Registry<T> owner, ResourceKey<T> key, T value) {
            this.owner = owner;
            this.holderOwner = owner;
            this.type = Type.STAND_ALONE;
            this.key = key;
            this.value = value;
        }

        public ResourceKey<T> key() {
            return key;
        }

        @Override
        public T value() {
            return value;
        }

        @Override
        public boolean isBound() {
            return value != null;
        }

        @Override
        public boolean is(Identifier id) {
            return key.identifier().equals(id);
        }

        @Override
        public boolean is(ResourceKey<T> other) {
            return key == other;
        }

        @Override
        public boolean is(Predicate<ResourceKey<T>> predicate) {
            return predicate.test(key);
        }

        @Override
        public boolean is(TagKey<T> tag) {
            return owner != null && owner.get(tag).map(set -> set.contains(this)).orElse(false);
        }

        @Override
        public boolean is(Holder<T> other) {
            return other == this || other.value() == value;
        }

        @Override
        public Optional<ResourceKey<T>> unwrapKey() {
            return Optional.of(key);
        }

        @Override
        public String toString() {
            return "Reference{" + key + "=" + value + "}";
        }
            protected Reference(Type type, HolderOwner<T> owner, ResourceKey<T> key, T value) {
            this.owner = owner instanceof Registry<T> registry ? registry : null;
            this.holderOwner = owner;
            this.type = type;
            this.key = key;
            this.value = value;
        }
        public static <T> Reference<T> createStandAlone(HolderOwner<T> owner, ResourceKey<T> key) {
            return new Reference<>(Type.STAND_ALONE, owner, key, null);
        }
        public static <T> Reference<T> createIntrusive(HolderOwner<T> owner, T value) {
            return new Reference<>(Type.INTRUSIVE, owner, null, value);
        }
        public boolean canSerializeIn(HolderOwner<T> context) {
            return holderOwner == null || holderOwner.canSerializeIn(context);
        }
        public Either<ResourceKey<T>, T> unwrap() {
            return Either.left(key);
        }
        public Kind kind() {
            return Kind.REFERENCE;
        }
        public boolean areComponentsBound() {
            return components != null;
        }
        void bindKey(ResourceKey<T> key) {
            if (this.key != null && key != this.key) {
                throw new IllegalStateException("Can't change holder key: existing=" + this.key + ", new=" + key);
            }
            this.key = key;
        }
        protected void bindValue(T value) {
            if (type == Type.INTRUSIVE && this.value != null && this.value != value) {
                throw new IllegalStateException("Can't change holder " + key + " value: existing=" + this.value + ", new=" + value);
            }
            this.value = value;
        }
        void bindTags(Collection<TagKey<T>> tags) {
            boundTags = java.util.Set.copyOf(tags);
        }
        public void bindComponents(DataComponentMap components) {
            this.components = components;
        }
        public Stream<TagKey<T>> tags() {
            return boundTags.stream();
        }
        public DataComponentMap components() {
            return components == null ? DataComponentMap.EMPTY : components;
        }
        protected static enum Type {
            STAND_ALONE, INTRUSIVE;
        }
}
    default boolean areComponentsBound() {
        return false;
    }
    default Stream<TagKey<T>> tags() {
        return Stream.of();
    }
    default DataComponentMap components() {
        return DataComponentMap.EMPTY;
    }
    default Either<ResourceKey<T>, T> unwrap() {
        return unwrapKey().<Either<ResourceKey<T>, T>>map(Either::left).orElseGet(() -> Either.right(value()));
    }
    default Kind kind() {
        return Kind.REFERENCE;
    }
    default boolean canSerializeIn(HolderOwner<T> registry) {
        return true;
    }
    default Optional<String> getRegisteredNameIfPresent() {
        return unwrapKey().map(key -> key.identifier().toString());
    }
    default String getRegisteredName() {
        return unwrapKey().map(key -> key.identifier().toString()).orElse("[unregistered]");
    }
    static <T> Holder<T> direct(T value, DataComponentMap components) {
        return new Direct<>(value);
    }
    public static enum Kind {
        REFERENCE, DIRECT;
    }
}
