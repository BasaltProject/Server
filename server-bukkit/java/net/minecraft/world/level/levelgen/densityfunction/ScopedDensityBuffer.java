// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

public class ScopedDensityBuffer extends DensityBuffer implements AutoCloseable {
    private final DensityBufferArena arena;
    private int age;
    private boolean closed;

    ScopedDensityBuffer(DensityBufferArena arena, int capacity, int size) {
        super(capacity);
        this.size = size;
        this.arena = arena;
    }

    void restore(int size) {
        if (size > values.length) {
            throw new IllegalArgumentException("The size is larger than the buffer capacity");
        }
        if (!closed) {
            throw new IllegalArgumentException("The buffer is in use");
        }
        age = 0;
        closed = false;
        this.size = size;
    }

    int incrementAge() {
        return ++age;
    }

    public void close() {
        if (!closed) {
            closed = true;
            arena.release(this);
        }
    }
}
