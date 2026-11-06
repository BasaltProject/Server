// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.Function;

public class SingleKeyCache<K, V> {
    private final Function<K, V> computeValue;
    private K cacheKey;
    private V cachedValue;

    public SingleKeyCache(Function<K, V> computeValue) {
        this.computeValue = computeValue;
    }

    public V getValue(K key) {
        if (cachedValue == null || !java.util.Objects.equals(cacheKey, key)) {
            cachedValue = computeValue.apply(key);
            cacheKey = key;
        }
        return cachedValue;
    }
}
