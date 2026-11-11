// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.ArrayList;
import java.util.List;

public interface Term<S> {
    boolean parse(ParseState<S> state, Scope scope, Control control);

    static <S, T> Term<S> marker(Atom<T> name, T value) {
        return new Marker<>(name, value);
    }

    @SafeVarargs
    static <S> Term<S> sequence(Term<S>... terms) {
        return new Sequence<>(terms);
    }

    @SafeVarargs
    static <S> Term<S> alternative(Term<S>... terms) {
        return new Alternative<>(terms);
    }

    static <S> Term<S> optional(Term<S> term) {
        return new Maybe<>(term);
    }

    static <S, T> Term<S> repeated(NamedRule<S, T> element, Atom<List<T>> listName) {
        return repeated(element, listName, 0);
    }

    static <S, T> Term<S> repeated(NamedRule<S, T> element, Atom<List<T>> listName, int minRepetitions) {
        return new Repeated<>(element, listName, minRepetitions);
    }

    static <S, T> Term<S> repeatedWithTrailingSeparator(NamedRule<S, T> element, Atom<List<T>> listName, Term<S> separator) {
        return repeatedWithTrailingSeparator(element, listName, separator, 0);
    }

    static <S, T> Term<S> repeatedWithTrailingSeparator(NamedRule<S, T> element, Atom<List<T>> listName, Term<S> separator, int minRepetitions) {
        return new RepeatedWithSeparator<>(element, listName, separator, minRepetitions, true);
    }

    static <S, T> Term<S> repeatedWithoutTrailingSeparator(NamedRule<S, T> element, Atom<List<T>> listName, Term<S> separator) {
        return repeatedWithoutTrailingSeparator(element, listName, separator, 0);
    }

    static <S, T> Term<S> repeatedWithoutTrailingSeparator(NamedRule<S, T> element, Atom<List<T>> listName, Term<S> separator, int minRepetitions) {
        return new RepeatedWithSeparator<>(element, listName, separator, minRepetitions, false);
    }

    static <S> Term<S> positiveLookahead(Term<S> term) {
        return new LookAhead<>(term, true);
    }

    static <S> Term<S> negativeLookahead(Term<S> term) {
        return new LookAhead<>(term, false);
    }

    static <S> Term<S> cut() {
        return (state, scope, control) -> {
            control.cut();
            return true;
        };
    }

    static <S> Term<S> empty() {
        return (state, scope, control) -> true;
    }

    static <S> Term<S> fail(Object message) {
        return (state, scope, control) -> {
            state.errorCollector().store(state.mark(), message);
            return false;
        };
    }

    public static record Marker<S, T>(Atom<T> name, T value) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            scope.put(name, value);
            return true;
        }
    }

    public static record Sequence<S>(Term<S>[] elements) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            int mark = state.mark();
            for (Term<S> element : elements) {
                if (!element.parse(state, scope, control)) {
                    state.restore(mark);
                    return false;
                }
            }
            return true;
        }
    }

    public static record Alternative<S>(Term<S>[] elements) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            Control inner = state.acquireControl();
            try {
                int mark = state.mark();
                for (Term<S> element : elements) {
                    scope.splitFrame();
                    if (element.parse(state, scope, inner)) {
                        scope.mergeFrame();
                        return true;
                    }
                    scope.popFrame();
                    state.restore(mark);
                    if (inner.hasCut()) {
                        break;
                    }
                }
                return false;
            } finally {
                state.releaseControl();
            }
        }
    }

    public static record Maybe<S>(Term<S> term) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            int mark = state.mark();
            scope.splitFrame();
            if (term.parse(state, scope, control)) {
                scope.mergeFrame();
            } else {
                scope.popFrame();
                state.restore(mark);
            }
            return true;
        }
    }

    public static record Repeated<S, T>(NamedRule<S, T> element, Atom<List<T>> listName, int minRepetitions) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            List<T> values = new ArrayList<>();
            while (true) {
                int mark = state.mark();
                T value = state.parse(element);
                if (value == null) {
                    state.restore(mark);
                    break;
                }
                values.add(value);
            }
            if (values.size() < minRepetitions) {
                return false;
            }
            scope.put(listName, values);
            return true;
        }
    }

    public static record RepeatedWithSeparator<S, T>(NamedRule<S, T> element, Atom<List<T>> listName, Term<S> separator, int minRepetitions, boolean allowTrailingSeparator) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            List<T> values = new ArrayList<>();
            boolean first = true;
           
            while (true) {
                int mark = state.mark();
                if (!first && !separator.parse(state, scope, control)) {
                    state.restore(mark);
                    break;
                }
                int afterSeparator = state.mark();
                T value = state.parse(element);
                if (value == null) {
                    state.restore(first || allowTrailingSeparator ? afterSeparator : mark);
                    break;
                }
                values.add(value);
                first = false;
            }
            if (values.size() < minRepetitions) {
                return false;
            }
            scope.put(listName, values);
            return true;
        }
    }

    public static record LookAhead<S>(Term<S> term, boolean positive) implements Term<S> {
        @Override
        public boolean parse(ParseState<S> state, Scope scope, Control control) {
            int mark = state.mark();
            boolean matched = term.parse(state.silent(), scope, control);
            state.restore(mark);
            return matched == positive;
        }
    }
}
