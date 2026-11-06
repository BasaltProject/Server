// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

public interface DensityBufferArena {
    DensityBufferArena GLOBAL = new DensityBufferArena() {
        public ScopedDensityBuffer acquire(int size) {
            return new ScopedDensityBuffer(this, size, size);
        }

        public void release(ScopedDensityBuffer buffer) {
        }
    };

    ScopedDensityBuffer acquire(int size);

    void release(ScopedDensityBuffer buffer);
}
