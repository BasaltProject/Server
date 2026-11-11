// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
import java.util.Objects;
@FunctionalInterface
public interface CharPredicate {
    boolean test(char value);
    default CharPredicate and(CharPredicate other) {
        Objects.requireNonNull(other);
        return c -> test(c) && other.test(c);
    }
    default CharPredicate negate() {
        return c -> !test(c);
    }
    default CharPredicate or(CharPredicate other) {
        Objects.requireNonNull(other);
        return c -> test(c) || other.test(c);
    }
}
