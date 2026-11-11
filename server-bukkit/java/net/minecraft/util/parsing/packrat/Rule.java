// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

public interface Rule<S, T> {
    T parse(ParseState<S> state);

    static <S, T> Rule<S, T> fromTerm(Term<S> child, RuleAction<S, T> action) {
        return new WrappedTerm<>(action, child);
    }

    static <S, T> Rule<S, T> fromTerm(Term<S> child, SimpleRuleAction<S, T> action) {
        return new WrappedTerm<>(action, child);
    }

    public interface SimpleRuleAction<S, T> extends RuleAction<S, T> {
        T run(Scope ruleScope);

        @Override
        default T run(ParseState<S> state) {
            return run(state.scope());
        }
    }

    public static record WrappedTerm<S, T>(RuleAction<S, T> action, Term<S> child) implements Rule<S, T> {
        @Override
        public T parse(ParseState<S> state) {
            Scope scope = state.scope();
            scope.pushFrame();
            try {
                if (!child.parse(state, scope, Control.UNBOUND)) {
                    return null;
                }
                return action.run(state);
            } finally {
                scope.popFrame();
            }
        }
    }

    public interface RuleAction<S, T> {
        T run(ParseState<S> state);
    }
}
