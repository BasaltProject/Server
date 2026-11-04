// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.AbstractSet;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class SortedArraySet<T> extends AbstractSet<T> {
    private final Comparator<T> comparator;
    private Object[] contents;
    private int size;

    private SortedArraySet(int initialCapacity, Comparator<T> comparator) {
        this.comparator = comparator;
        if (initialCapacity < 0) {
            throw new IllegalArgumentException("Initial capacity (" + initialCapacity + ") is negative");
        }
        this.contents = new Object[initialCapacity];
    }

    public static <T extends Comparable<T>> SortedArraySet<T> create() {
        return create(10);
    }

    public static <T extends Comparable<T>> SortedArraySet<T> create(int initialCapacity) {
        return new SortedArraySet<>(initialCapacity, Comparator.<T>naturalOrder());
    }

    public static <T> SortedArraySet<T> create(Comparator<T> comparator) {
        return create(comparator, 10);
    }

    public static <T> SortedArraySet<T> create(Comparator<T> comparator, int initialCapacity) {
        return new SortedArraySet<>(initialCapacity, comparator);
    }

    @SuppressWarnings("unchecked")
    private T at(int index) {
        return (T) contents[index];
    }

    private int findIndex(T t) {
        return Arrays.binarySearch(contents, 0, size, t, (Comparator<Object>) comparator);
    }

    private static int insertionPoint(int found) {
        return -found - 1;
    }

    @Override
    public boolean add(T t) {
        int found = findIndex(t);
        if (found >= 0) {
            return false;
        }
        int at = insertionPoint(found);
        if (size == contents.length) {
            contents = Arrays.copyOf(contents, Math.max(contents.length * 2, 4));
        }
        System.arraycopy(contents, at, contents, at + 1, size - at);
        contents[at] = t;
        size++;
        return true;
    }

    public T addOrGet(T t) {
        int found = findIndex(t);
        if (found >= 0) {
            return at(found);
        }
        add(t);
        return t;
    }

    @SuppressWarnings("unchecked")
    @Override
    public boolean remove(Object o) {
        int found = findIndex((T) o);
        if (found < 0) {
            return false;
        }
        System.arraycopy(contents, found + 1, contents, found, size - found - 1);
        contents[--size] = null;
        return true;
    }

    public T get(T t) {
        int found = findIndex(t);
        return found >= 0 ? at(found) : null;
    }

    public T first() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return at(0);
    }

    public T last() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return at(size - 1);
    }

    @SuppressWarnings("unchecked")
    @Override
    public boolean contains(Object o) {
        return findIndex((T) o) >= 0;
    }

    @Override
    public Iterator<T> iterator() {
        return new ArrayIterator();
    }

    @Override
    public int size() {
        return size;
    }

    @Override
    public Object[] toArray() {
        return Arrays.copyOf(contents, size);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <U> U[] toArray(U[] a) {
        if (a.length < size) {
            return (U[]) Arrays.copyOf(contents, size, a.getClass());
        }
        System.arraycopy(contents, 0, a, 0, size);
        if (a.length > size) {
            a[size] = null;
        }
        return a;
    }

    @Override
    public void clear() {
        Arrays.fill(contents, 0, size, null);
        size = 0;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof SortedArraySet<?> other && comparator.equals(other.comparator)) {
            return size == other.size && Arrays.equals(contents, 0, size, other.contents, 0, size);
        }
        return super.equals(o);
    }

    @Override
    public int hashCode() {
        return super.hashCode();
    }

    private class ArrayIterator implements Iterator<T> {
        private int index;
        private int last = -1;

        private ArrayIterator() {}

        @Override
        public boolean hasNext() {
            return index < size;
        }

        @Override
        public T next() {
            if (index >= size) {
                throw new NoSuchElementException();
            }
            last = index++;
            return at(last);
        }

        @Override
        public void remove() {
            if (last == -1) {
                throw new IllegalStateException();
            }
            System.arraycopy(contents, last + 1, contents, last, size - last - 1);
            size--;
            contents[size] = null;
            index = last;
            last = -1;
        }
    }
}
