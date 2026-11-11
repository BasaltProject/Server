// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.Arrays;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;
import net.minecraft.core.IdMap;

public class CrudeIncrementalIntIdentityHashBiMap<K> implements IdMap<K> {
    private static final int kNotFound = -1;
    private Map<K, Integer> ids;
    private Object[] byId;
    private int nextId;

    private CrudeIncrementalIntIdentityHashBiMap(int capacity) {
        this.ids = new IdentityHashMap<>(capacity);
        this.byId = new Object[Math.max(capacity, 4)];
    }

    public static <A> CrudeIncrementalIntIdentityHashBiMap<A> create(int initialCapacity) {
        return new CrudeIncrementalIntIdentityHashBiMap<>((int) (initialCapacity / 0.8F));
    }

    @Override
    public int getId(K thing) {
        Integer id = ids.get(thing);
        return id == null ? kNotFound : id;
    }

    @Override
    @SuppressWarnings("unchecked")
    public K byId(int id) {
        return id >= 0 && id < byId.length ? (K) byId[id] : null;
    }

    public boolean contains(K key) {
        return ids.containsKey(key);
    }

    public boolean contains(int id) {
        return byId(id) != null;
    }

    public int add(K key) {
        while (nextId < byId.length && byId[nextId] != null) {
            nextId++;
        }
        int id = nextId;
        addMapping(key, id);
        return id;
    }

    public void addMapping(K key, int id) {
        if (id >= byId.length) {
            int size = byId.length;
            while (size <= id) {
                size <<= 1;
            }
            byId = Arrays.copyOf(byId, size);
        }
        ids.put(key, id);
        byId[id] = key;
        if (id == nextId) {
            nextId++;
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public Iterator<K> iterator() {
        Object[] snapshot = byId;
        return new Iterator<>() {
            private int index = advance(0);

            private int advance(int from) {
                int i = from;
                while (i < snapshot.length && snapshot[i] == null) {
                    i++;
                }
                return i;
            }

            @Override
            public boolean hasNext() {
                return index < snapshot.length;
            }

            @Override
            public K next() {
                if (!hasNext()) {
                    throw new NoSuchElementException();
                }
                K value = (K) snapshot[index];
                index = advance(index + 1);
                return value;
            }
        };
    }

    public void clear() {
        ids.clear();
        Arrays.fill(byId, null);
        nextId = 0;
    }

    @Override
    public int size() {
        return ids.size();
    }

    public CrudeIncrementalIntIdentityHashBiMap<K> copy() {
        CrudeIncrementalIntIdentityHashBiMap<K> clone = new CrudeIncrementalIntIdentityHashBiMap<>(byId.length);
        clone.ids.putAll(ids);
        clone.byId = byId.clone();
        clone.nextId = nextId;
        return clone;
    }
}
