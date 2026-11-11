// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.Optional;

public interface ParseState<S> {
    Scope scope();

    ErrorCollector<S> errorCollector();

    default <T> Optional<T> parseTopRule(NamedRule<S, T> rule) {
        T value = parse(rule);
        if (value != null) {
            errorCollector().finish(mark());
        }
        return Optional.ofNullable(value);
    }

    <T> T parse(NamedRule<S, T> rule);

    S input();

    int mark();

    void restore(int mark);

    Control acquireControl();

    void releaseControl();

    ParseState<S> silent();
}
