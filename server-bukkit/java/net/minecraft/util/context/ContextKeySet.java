// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.context;

import java.util.HashSet;
import java.util.Set;

public class ContextKeySet {
    private final Set<ContextKey<?>> required;
    private final Set<ContextKey<?>> allowed;

    private ContextKeySet(Set<ContextKey<?>> required, Set<ContextKey<?>> optional) {
        this.required = Set.copyOf(required);
        Set<ContextKey<?>> all = new HashSet<>(required);
        all.addAll(optional);
        this.allowed = Set.copyOf(all);
    }

    public Set<ContextKey<?>> required() {
        return required;
    }

    public Set<ContextKey<?>> allowed() {
        return allowed;
    }

    @Override
    public String toString() {
        return "[" + String.join(", ", allowed.stream().map(key -> (required.contains(key) ? "!" : "") + key.name()).toList()) + "]";
    }

    public static class Builder {
        private final Set<ContextKey<?>> required = new HashSet<>();
        private final Set<ContextKey<?>> optional = new HashSet<>();

        public Builder required(ContextKey<?> param) {
            if (optional.contains(param)) {
                throw new IllegalArgumentException("Parameter " + param + " is already optional");
            }
            required.add(param);
            return this;
        }

        public Builder optional(ContextKey<?> param) {
            if (required.contains(param)) {
                throw new IllegalArgumentException("Parameter " + param + " is already required");
            }
            optional.add(param);
            return this;
        }

        public ContextKeySet build() {
            return new ContextKeySet(required, optional);
        }
    }
}
