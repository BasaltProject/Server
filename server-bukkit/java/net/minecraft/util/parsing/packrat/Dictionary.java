// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

public class Dictionary<S> {
    private final Map<Atom<?>, Entry<S, ?>> terms = new HashMap<>();

    @SuppressWarnings("unchecked")
    private <T> Entry<S, T> entry(Atom<T> name) {
        return (Entry<S, T>) terms.computeIfAbsent(name, key -> new Entry<S, T>((Atom<T>) key));
    }

    public <T> NamedRule<S, T> put(Atom<T> name, Rule<S, T> entry) {
        Entry<S, T> slot = entry(name);
        if (slot.value != null) {
            throw new IllegalArgumentException("Trying to override rule: " + name);
        }
        slot.value = entry;
        return slot;
    }

    public <T> NamedRule<S, T> putComplex(Atom<T> name, Term<S> term, Rule.RuleAction<S, T> action) {
        return put(name, Rule.fromTerm(term, action));
    }

    public <T> NamedRule<S, T> put(Atom<T> name, Term<S> term, Rule.SimpleRuleAction<S, T> action) {
        return put(name, Rule.fromTerm(term, action));
    }

    public void checkAllBound() {
        StringBuilder unbound = new StringBuilder();
        for (Entry<S, ?> slot : terms.values()) {
            if (slot.value == null) {
                unbound.append(' ').append(slot.name);
            }
        }
        if (!unbound.isEmpty()) {
            throw new IllegalStateException("Unbound rules:" + unbound);
        }
    }

    public <T> NamedRule<S, T> getOrThrow(Atom<T> name) {
        @SuppressWarnings("unchecked")
        Entry<S, T> slot = (Entry<S, T>) terms.get(name);
        if (slot == null) {
            throw new IllegalArgumentException("No rule called " + name);
        }
        return slot;
    }

    public <T> NamedRule<S, T> forward(Atom<T> name) {
        return entry(name);
    }

    public <T> Term<S> named(Atom<T> name) {
        return new Reference<>(entry(name), name);
    }

    public <T> Term<S> namedWithAlias(Atom<T> nameToParse, Atom<T> nameToStore) {
        return new Reference<>(entry(nameToParse), nameToStore);
    }

    static record Reference<S, T>(Entry<S, T> ruleToParse, Atom<T> nameToStore) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            T value = state.parse(ruleToParse);
            if (value == null) {
                return false;
            }
            scope.put(nameToStore, value);
            return true;
        }
    }

    static class Entry<S, T> implements NamedRule<S, T>, Supplier<String> {
        private final Atom<T> name;
        private Rule<S, T> value;

        private Entry(Atom<T> name) {
            this.name = name;
        }

        @Override
        public Atom<T> name() {
            return name;
        }

        @Override
        public Rule<S, T> value() {
            if (value == null) {
                throw new IllegalStateException("Unbound rule " + name);
            }
            return value;
        }

        @Override
        public String get() {
            return name.toString();
        }
    }
}
