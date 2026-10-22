package net.minecraft.core;

import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

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
    }

    class Reference<T> implements Holder<T> {
        private final ResourceKey<T> key;
        private final T value;
        private final Registry<T> owner;

        public Reference(Registry<T> owner, ResourceKey<T> key, T value) {
            this.owner = owner;
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
    }
}
