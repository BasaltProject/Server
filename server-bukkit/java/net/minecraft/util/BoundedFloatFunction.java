// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.Function;

public interface BoundedFloatFunction<C> {
    BoundedFloatFunction<Float> IDENTITY = new BoundedFloatFunction<>() {
        public float apply(Float value) {
            return value;
        }

        public Interval range() {
            return Interval.INFINITE;
        }
    };

    float apply(C c);

    Interval range();

    static <C> BoundedFloatFunction<C> constant(float value) {
        Interval fixed = Interval.ofExact(value);
        return new BoundedFloatFunction<>() {
            public float apply(C c) {
                return value;
            }

            public Interval range() {
                return fixed;
            }
        };
    }

    default <C2> BoundedFloatFunction<C2> comap(Function<C2, C> function) {
        BoundedFloatFunction<C> inner = this;
        return new BoundedFloatFunction<>() {
            public float apply(C2 c2) {
                return inner.apply(function.apply(c2));
            }

            public Interval range() {
                return inner.range();
            }
        };
    }
}
