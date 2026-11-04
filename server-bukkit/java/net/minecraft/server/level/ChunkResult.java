// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

public interface ChunkResult<T> {
    static <T> ChunkResult<T> of(T value) {
        return new Success<>(value);
    }

    static <T> ChunkResult<T> error(String message) {
        return new Fail<>(message);
    }

    boolean isSuccess();

    T orElse(T orElse);

    T orElseThrow(java.util.function.Supplier<? extends RuntimeException> supplier);

    record Success<T>(T value) implements ChunkResult<T> {
        @Override
        public boolean isSuccess() {
            return true;
        }

        @Override
        public T orElse(T orElse) {
            return value;
        }

        @Override
        public T orElseThrow(java.util.function.Supplier<? extends RuntimeException> supplier) {
            return value;
        }
    }

    record Fail<T>(String message) implements ChunkResult<T> {
        @Override
        public boolean isSuccess() {
            return false;
        }

        @Override
        public T orElse(T orElse) {
            return orElse;
        }

        @Override
        public T orElseThrow(java.util.function.Supplier<? extends RuntimeException> supplier) {
            throw supplier.get();
        }
    }

    static <T> ChunkResult<T> error(Supplier<String> errorSupplier) {
        return new Fail<>(errorSupplier.get());
    }
    static <R> R orElse(ChunkResult<? extends R> chunkResult, R orElse) {
        return chunkResult.isSuccess() ? chunkResult.orElse(null) : orElse;
    }
    default String getError() {
        return this instanceof Fail<T> fail ? fail.message() : null;
    }
    default ChunkResult<T> ifSuccess(Consumer<T> consumer) {
        if (this instanceof Success<T> success) {
            consumer.accept(success.value());
        }
        return this;
    }
    default <R> ChunkResult<R> map(Function<T, R> map) {
        return this instanceof Success<T> success ? ChunkResult.of(map.apply(success.value())) : ChunkResult.error(getError());
    }
}
