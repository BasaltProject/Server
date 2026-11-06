// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

public class DependencySorter<K, V extends DependencySorter.Entry<K>> {
    private final Map<K, V> contents = new HashMap<>();

    public DependencySorter<K, V> addEntry(K id, V value) {
        contents.put(id, value);
        return this;
    }

    /** Emits each entry after the entries it requires; entries in a cycle or missing a required one are skipped. */
    public void orderByDependencies(BiConsumer<K, V> output) {
        Set<K> done = new HashSet<>();
        Set<K> skipped = new HashSet<>();
        Map<K, Set<K>> visiting = new LinkedHashMap<>();
        for (K id : contents.keySet()) {
            visit(id, output, done, skipped, visiting);
        }
    }

    private boolean visit(K id, BiConsumer<K, V> output, Set<K> done, Set<K> skipped, Map<K, Set<K>> visiting) {
        if (done.contains(id)) {
            return true;
        }
        if (skipped.contains(id) || visiting.containsKey(id)) {
            return false;
        }
        V entry = contents.get(id);
        if (entry == null) {
            skipped.add(id);
            return false;
        }
        visiting.put(id, Set.of());
        boolean[] ready = {true};
        entry.visitRequiredDependencies(dependency -> ready[0] &= visit(dependency, output, done, skipped, visiting));
        entry.visitOptionalDependencies(dependency -> {
            if (contents.containsKey(dependency)) {
                visit(dependency, output, done, skipped, visiting);
            }
        });
        visiting.remove(id);
        if (!ready[0]) {
            skipped.add(id);
            return false;
        }
        done.add(id);
        output.accept(id, entry);
        return true;
    }

    public interface Entry<K> {
        void visitRequiredDependencies(Consumer<K> output);

        void visitOptionalDependencies(Consumer<K> output);
    }
}
