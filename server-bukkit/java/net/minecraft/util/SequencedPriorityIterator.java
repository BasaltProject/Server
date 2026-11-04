// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.google.common.collect.AbstractIterator;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Map;
import java.util.TreeMap;
public final class SequencedPriorityIterator<T> extends AbstractIterator<T> {
    private final TreeMap<Integer, Deque<T>> queues = new TreeMap<>();
    public void add(T data, int priority) {
        queues.computeIfAbsent(priority, key -> new ArrayDeque<>()).addLast(data);
    }
    protected T computeNext() {
        Map.Entry<Integer, Deque<T>> highest = queues.lastEntry();
        if (highest == null) {
            return endOfData();
        }
        T result = highest.getValue().removeFirst();
        if (highest.getValue().isEmpty()) {
            queues.remove(highest.getKey());
        }
        return result;
    }
}
