// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class Scope {
    private final List<Map<Atom<?>, Object>> frames = new ArrayList<>();

    public Scope() {
        frames.add(new HashMap<>());
    }

    private Map<Atom<?>, Object> top() {
        return frames.get(frames.size() - 1);
    }

    public int valueIndexForAny(Atom<?>... atoms) {
        for (int index = 0; index < atoms.length; index++) {
            if (top().containsKey(atoms[index])) {
                return index;
            }
        }
        return -1;
    }

    public void pushFrame() {
        frames.add(new HashMap<>());
    }

    public void popFrame() {
        frames.remove(frames.size() - 1);
    }

    public void splitFrame() {
        frames.add(new HashMap<>(top()));
    }

    public void clearFrameValues() {
        top().clear();
    }

    public void mergeFrame() {
        Map<Atom<?>, Object> merged = frames.remove(frames.size() - 1);
        frames.set(frames.size() - 1, merged);
    }

    public <T> void put(Atom<T> name, T value) {
        top().put(name, value);
    }

    @SuppressWarnings("unchecked")
    public <T> T get(Atom<T> name) {
        return (T) top().get(name);
    }

    public <T> T getOrThrow(Atom<T> name) {
        T value = get(name);
        if (value == null) {
            throw new IllegalArgumentException("No value for atom " + name);
        }
        return value;
    }

    public <T> T getOrDefault(Atom<T> name, T fallback) {
        T value = get(name);
        return value == null ? fallback : value;
    }

    @SafeVarargs
    public final <T> T getAny(Atom<? extends T>... names) {
        int index = valueIndexForAny(names);
        return index < 0 ? null : get(names[index]);
    }

    @SafeVarargs
    public final <T> T getAnyOrThrow(Atom<? extends T>... names) {
        T value = getAny(names);
        if (value == null) {
            throw new IllegalArgumentException("No value for any of the atoms");
        }
        return value;
    }

    @Override
    public String toString() {
        return "Scope" + top();
    }

    public Map<Atom<?>, ?> lastFrame() {
        return top();
    }

    public boolean hasOnlySingleFrame() {
        return frames.size() == 1;
    }
}
