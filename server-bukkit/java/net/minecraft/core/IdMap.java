package net.minecraft.core;

public interface IdMap<T> extends Iterable<T> {
    int getId(T thing);

    T byId(int id);

    default T byIdOrThrow(int id) {
        T value = byId(id);
        if (value == null) {
            throw new IllegalArgumentException("No value with id " + id);
        }
        return value;
    }

    default int getIdOrThrow(T thing) {
        int id = getId(thing);
        if (id == -1) {
            throw new IllegalArgumentException("Can't find id for '" + thing + "' in map " + this);
        }
        return id;
    }

    int size();
}
