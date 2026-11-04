// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import java.io.Serializable;
import java.util.Deque;
import java.util.List;
import java.util.RandomAccess;
public interface ListAndDeque<T> extends List<T>, RandomAccess, Cloneable, Serializable, Deque<T> {
    ListAndDeque<T> reversed();
    T getFirst();
    T getLast();
    void addFirst(T t);
    void addLast(T t);
    T removeFirst();
    T removeLast();
    default boolean offer(T value) {
        return add(value);
    }
    default T remove() {
        return removeFirst();
    }
    default T poll() {
        return isEmpty() ? null : removeFirst();
    }
    default T element() {
        return getFirst();
    }
    default T peek() {
        return isEmpty() ? null : getFirst();
    }
    default void push(T value) {
        addFirst(value);
    }
    default T pop() {
        return removeFirst();
    }
}
