// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.Consumer;

public interface AbortableIterationConsumer<T> {
    Continuation accept(T entry);

    static <T> AbortableIterationConsumer<T> forConsumer(Consumer<T> consumer) {
        return entry -> {
            consumer.accept(entry);
            return Continuation.CONTINUE;
        };
    }

    enum Continuation {
        CONTINUE,
        ABORT;

        public boolean shouldAbort() {
            return this == ABORT;
        }
    }
}
