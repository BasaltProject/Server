// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.context;

import java.util.HashMap;
import java.util.Map;

public final class ContextMap {
    public static final ContextMap EMPTY = new ContextMap(Map.of());
    private final Map<ContextKey<?>, Object> params;

    private ContextMap(Map<ContextKey<?>, Object> params) {
        this.params = params;
    }

    public static Builder builder() {
        return new Builder();
    }

    public boolean has(ContextKey<?> key) {
        return params.containsKey(key);
    }

    public <T> T getOrThrow(ContextKey<T> key) {
        T value = get(key);
        if (value == null) {
            throw new NoSuchElementException(key.name().toString());
        }
        return value;
    }

    @SuppressWarnings("unchecked")
    public <T> T get(ContextKey<T> key) {
        return (T) params.get(key);
    }

    public <T> T getOrDefault(ContextKey<T> param, T fallback) {
        T value = get(param);
        return value != null ? value : fallback;
    }

    public static class NoSuchElementException extends java.util.NoSuchElementException {
        NoSuchElementException(String message) {
            super(message);
        }
    }

    public static class Builder {
        private final Map<ContextKey<?>, Object> params = new HashMap<>();

        private Builder() {
        }

        public <T> Builder set(ContextKey<T> param, T value) {
            params.put(param, value);
            return this;
        }

        @SuppressWarnings("unchecked")
        public <T> T get(ContextKey<T> param) {
            return (T) params.get(param);
        }

        public ContextMap build() {
            return new ContextMap(Map.copyOf(params));
        }

        public ContextMap buildAndValidate(ContextKeySet paramSet) {
            for (ContextKey<?> key : paramSet.required()) {
                if (!params.containsKey(key)) {
                    throw new IllegalArgumentException("Missing required parameters: " + key);
                }
            }
            for (ContextKey<?> key : params.keySet()) {
                if (!paramSet.allowed().contains(key)) {
                    throw new IllegalArgumentException("Parameter " + key + " is not allowed");
                }
            }
            return build();
        }
    }
}
