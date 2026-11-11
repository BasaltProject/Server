// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.nbt.Tag;

public class EncoderCache {
    private final Map<Key<?, ?>, DataResult<?>> cache;

    public EncoderCache(int maximumSize) {
        this.cache = new LinkedHashMap<>(16, 0.75F, true) {
            @Override
            protected boolean removeEldestEntry(Map.Entry<Key<?, ?>, DataResult<?>> eldest) {
                return size() > maximumSize;
            }
        };
    }

    public <A> Codec<A> wrap(Codec<A> codec) {
        return new Codec<A>() {
            @Override
            public <T> DataResult<Pair<A, T>> decode(DynamicOps<T> ops, T input) {
                return codec.decode(ops, input);
            }

            @Override
            @SuppressWarnings("unchecked")
            public <T> DataResult<T> encode(A input, DynamicOps<T> ops, T prefix) {
                Key<A, T> key = new Key<>(codec, input, ops);
                DataResult<T> cached;
                synchronized (cache) {
                    cached = (DataResult<T>) cache.computeIfAbsent(key, Key::resolve);
                }
                return cached.map(value -> value instanceof Tag tag ? (T) tag.copy() : value);
            }
        };
    }

    static record Key<A, T>(Codec<A> codec, A value, DynamicOps<T> ops) {
        public DataResult<T> resolve() {
            return codec.encodeStart(ops, value);
        }

        @Override
        public boolean equals(Object obj) {
            return this == obj || obj instanceof Key<?, ?> key && codec == key.codec && value.equals(key.value) && ops.equals(key.ops);
        }

        @Override
        public int hashCode() {
            return 31 * (31 * System.identityHashCode(codec) + value.hashCode()) + ops.hashCode();
        }
    }
}
