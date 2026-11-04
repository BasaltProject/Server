// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.common.collect.Iterators;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import java.util.AbstractCollection;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class ClassInstanceMultiMap<T> extends AbstractCollection<T> {
    private final Map<Class<?>, List<T>> byClass = Maps.newHashMap();
    private final Class<T> baseClass;
    private final List<T> allInstances = Lists.newArrayList();

    public ClassInstanceMultiMap(Class<T> baseClass) {
        this.baseClass = baseClass;
        this.byClass.put(baseClass, allInstances);
    }

    public boolean add(T instance) {
        boolean added = false;
        for (Map.Entry<Class<?>, List<T>> entry : byClass.entrySet()) {
            if (entry.getKey().isInstance(instance)) {
                added |= entry.getValue().add(instance);
            }
        }
        return added;
    }

    public boolean remove(Object object) {
        boolean removed = false;
        for (Map.Entry<Class<?>, List<T>> entry : byClass.entrySet()) {
            if (entry.getKey().isInstance(object)) {
                removed |= entry.getValue().remove(object);
            }
        }
        return removed;
    }

    public boolean contains(Object o) {
        return !find(o.getClass()).isEmpty() && find(o.getClass()).contains(o);
    }

    @SuppressWarnings("unchecked")
    public <S> Collection<S> find(Class<S> index) {
        if (!baseClass.isAssignableFrom(index)) {
            throw new IllegalArgumentException("Don't know how to search for " + index);
        }
        List<T> found = byClass.computeIfAbsent(index, type -> {
            List<T> matching = Lists.newArrayList();
            for (T instance : allInstances) {
                if (type.isInstance(instance)) {
                    matching.add(instance);
                }
            }
            return matching;
        });
        return (Collection<S>) Collections.unmodifiableCollection(found);
    }

    public Iterator<T> iterator() {
        return allInstances.isEmpty() ? Collections.emptyIterator() : Iterators.unmodifiableIterator(allInstances.iterator());
    }

    public List<T> getAllInstances() {
        return Collections.unmodifiableList(allInstances);
    }

    public int size() {
        return allInstances.size();
    }
}
