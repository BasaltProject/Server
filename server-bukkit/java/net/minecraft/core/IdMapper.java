package net.minecraft.core;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class IdMapper<T> implements IdMap<T> {
    private final Map<T, Integer> tToId = new IdentityHashMap<>();
    private final List<T> idToT = new ArrayList<>();

    public IdMapper() {}

    public void addMapping(T thing, int id) {
        tToId.put(thing, id);
        while (idToT.size() <= id) {
            idToT.add(null);
        }
        idToT.set(id, thing);
    }

    public void add(T thing) {
        addMapping(thing, idToT.size());
    }

    @Override
    public int getId(T thing) {
        Integer id = tToId.get(thing);
        return id == null ? -1 : id;
    }

    @Override
    public T byId(int id) {
        return id >= 0 && id < idToT.size() ? idToT.get(id) : null;
    }

    @Override
    public Iterator<T> iterator() {
        return idToT.stream().filter(java.util.Objects::nonNull).iterator();
    }

    public boolean contains(int id) {
        return byId(id) != null;
    }

    @Override
    public int size() {
        return tToId.size();
    }
}
