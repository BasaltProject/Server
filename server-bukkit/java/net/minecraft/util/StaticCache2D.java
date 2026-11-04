// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
public class StaticCache2D<T> {
    private final int minX;
    private final int minZ;
    private final int sizeX;
    private final int sizeZ;
    private final List<T> cache;
    public static <T> StaticCache2D<T> create(int centerX, int centerZ, int range, Initializer<T> initializer) {
        int side = range * 2 + 1;
        return new StaticCache2D<>(centerX - range, centerZ - range, side, side, initializer);
    }
    private StaticCache2D(int minX, int minZ, int sizeX, int sizeZ, Initializer<T> initializer) {
        this.minX = minX;
        this.minZ = minZ;
        this.sizeX = sizeX;
        this.sizeZ = sizeZ;
        this.cache = new ArrayList<>(sizeX * sizeZ);
        for (int x = minX; x < minX + sizeX; x++) {
            for (int z = minZ; z < minZ + sizeZ; z++) {
                cache.add(initializer.get(x, z));
            }
        }
    }
    public <U> StaticCache2D<U> map(MappingFunction<T, U> function) {
        return new StaticCache2D<>(minX, minZ, sizeX, sizeZ, (x, z) -> function.apply(get(x, z), x, z));
    }
    public void forEach(Consumer<T> consumer) {
        cache.forEach(consumer);
    }
    public T get(int x, int z) {
        if (!contains(x, z)) {
            throw new IllegalArgumentException("Requested out of range value (" + x + "," + z + ") from " + this);
        }
        return cache.get((x - minX) * sizeZ + (z - minZ));
    }
    public boolean contains(int x, int z) {
        return x >= minX && x < minX + sizeX && z >= minZ && z < minZ + sizeZ;
    }
    public String toString() {
        return "StaticCache2D[" + minX + ", " + minZ + ", " + (minX + sizeX) + ", " + (minZ + sizeZ) + "]";
    }
    public interface Initializer<T> {
        T get(int x, int z);
    }
    public interface MappingFunction<T, U> {
        U apply(T value, int x, int z);
    }
}
