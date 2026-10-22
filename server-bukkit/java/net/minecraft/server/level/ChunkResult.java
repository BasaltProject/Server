package net.minecraft.server.level;

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
}
