// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.AbstractList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.Objects;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.UnaryOperator;

public class ArrayListDeque<T> extends AbstractList<T> implements ListAndDeque<T> {
    private static final int DEFAULT_CAPACITY = 16;
    private Object[] contents;
    private int head;
    private int size;

    public ArrayListDeque() {
        this(DEFAULT_CAPACITY);
    }

    public ArrayListDeque(int capacity) {
        contents = new Object[Math.max(capacity, 1)];
    }

    private int wrap(int index) {
        return (head + index) % contents.length;
    }

    private void grow() {
        if (size < contents.length) {
            return;
        }
        Object[] bigger = new Object[contents.length * 2];
        for (int i = 0; i < size; i++) {
            bigger[i] = contents[wrap(i)];
        }
        contents = bigger;
        head = 0;
    }

    private void check(int index, int limit) {
        if (index < 0 || index >= limit) {
            throw new IndexOutOfBoundsException("Index " + index + " out of bounds for size " + size);
        }
    }

    @Override
    public int size() {
        return size;
    }

    public int capacity() {
        return contents.length;
    }

    @SuppressWarnings("unchecked")
    @Override
    public T get(int index) {
        check(index, size);
        return (T) contents[wrap(index)];
    }

    @Override
    public T set(int index, T element) {
        check(index, size);
        int slot = wrap(index);
        T previous = get(index);
        contents[slot] = element;
        return previous;
    }

    @Override
    public void add(int index, T element) {
        check(index, size + 1);
        grow();
        if (index == 0) {
            head = (head - 1 + contents.length) % contents.length;
            contents[head] = element;
        } else {
            for (int i = size; i > index; i--) {
                contents[wrap(i)] = contents[wrap(i - 1)];
            }
            contents[wrap(index)] = element;
        }
        size++;
        modCount++;
    }

    @Override
    public T remove(int index) {
        check(index, size);
        T removed = get(index);
        for (int i = index; i < size - 1; i++) {
            contents[wrap(i)] = contents[wrap(i + 1)];
        }
        contents[wrap(size - 1)] = null;
        size--;
        modCount++;
        return removed;
    }

    @Override
    public boolean removeIf(Predicate<? super T> filter) {
        boolean removed = false;
        int write = 0;
        int original = size;
        for (int read = 0; read < original; read++) {
            T value = get(read);
            if (filter.test(value)) {
                removed = true;
            } else {
                contents[wrap(write++)] = value;
            }
        }
        for (int i = write; i < original; i++) {
            contents[wrap(i)] = null;
        }
        size = write;
        if (removed) {
            modCount++;
        }
        return removed;
    }

    @Override
    public void replaceAll(UnaryOperator<T> operator) {
        for (int i = 0; i < size; i++) {
            set(i, operator.apply(get(i)));
        }
    }

    @Override
    public void forEach(Consumer<? super T> action) {
        for (int i = 0; i < size; i++) {
            action.accept(get(i));
        }
    }

    @Override
    public void addFirst(T value) {
        add(0, value);
    }

    @Override
    public void addLast(T value) {
        add(size, value);
    }

    @Override
    public boolean offerFirst(T value) {
        addFirst(value);
        return true;
    }

    @Override
    public boolean offerLast(T value) {
        addLast(value);
        return true;
    }

    @Override
    public T removeFirst() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return remove(0);
    }

    @Override
    public T removeLast() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return remove(size - 1);
    }

    @Override
    public ListAndDeque<T> reversed() {
        return new ReversedView(this);
    }

    @Override
    public T pollFirst() {
        return size == 0 ? null : removeFirst();
    }

    @Override
    public T pollLast() {
        return size == 0 ? null : removeLast();
    }

    @Override
    public T getFirst() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return get(0);
    }

    @Override
    public T getLast() {
        if (size == 0) {
            throw new NoSuchElementException();
        }
        return get(size - 1);
    }

    @Override
    public T peekFirst() {
        return size == 0 ? null : get(0);
    }

    @Override
    public T peekLast() {
        return size == 0 ? null : get(size - 1);
    }

    @Override
    public boolean removeFirstOccurrence(Object o) {
        int index = indexOf(o);
        if (index < 0) {
            return false;
        }
        remove(index);
        return true;
    }

    @Override
    public boolean removeLastOccurrence(Object o) {
        int index = lastIndexOf(o);
        if (index < 0) {
            return false;
        }
        remove(index);
        return true;
    }

    @Override
    public Iterator<T> descendingIterator() {
        return new DescendingIterator();
    }

    @Override
    public void clear() {
        Arrays.fill(contents, null);
        head = 0;
        size = 0;
        modCount++;
    }

    class DescendingIterator implements Iterator<T> {
        private int index = size - 1;

        public DescendingIterator() {}

        @Override
        public boolean hasNext() {
            return index >= 0;
        }

        @Override
        public T next() {
            if (index < 0) {
                throw new NoSuchElementException();
            }
            return get(index--);
        }

        @Override
        public void remove() {
            ArrayListDeque.this.remove(index + 1);
        }
    }

    class ReversedView extends AbstractList<T> implements ListAndDeque<T> {
        private final ArrayListDeque<T> source;

        public ReversedView(ArrayListDeque<T> source) {
            this.source = source;
        }

        private int mirror(int index) {
            return source.size() - 1 - index;
        }

        @Override
        public ListAndDeque<T> reversed() {
            return source;
        }

        @Override
        public T getFirst() {
            return source.getLast();
        }

        @Override
        public T getLast() {
            return source.getFirst();
        }

        @Override
        public void addFirst(T t) {
            source.addLast(t);
        }

        @Override
        public void addLast(T t) {
            source.addFirst(t);
        }

        @Override
        public boolean offerFirst(T t) {
            return source.offerLast(t);
        }

        @Override
        public boolean offerLast(T t) {
            return source.offerFirst(t);
        }

        @Override
        public T pollFirst() {
            return source.pollLast();
        }

        @Override
        public T pollLast() {
            return source.pollFirst();
        }

        @Override
        public T peekFirst() {
            return source.peekLast();
        }

        @Override
        public T peekLast() {
            return source.peekFirst();
        }

        @Override
        public T removeFirst() {
            return source.removeLast();
        }

        @Override
        public T removeLast() {
            return source.removeFirst();
        }

        @Override
        public boolean removeFirstOccurrence(Object o) {
            return source.removeLastOccurrence(o);
        }

        @Override
        public boolean removeLastOccurrence(Object o) {
            return source.removeFirstOccurrence(o);
        }

        @Override
        public Iterator<T> descendingIterator() {
            return source.iterator();
        }

        @Override
        public int size() {
            return source.size();
        }

        @Override
        public boolean isEmpty() {
            return source.isEmpty();
        }

        @Override
        public boolean contains(Object o) {
            return source.contains(o);
        }

        @Override
        public T get(int index) {
            return source.get(mirror(index));
        }

        @Override
        public T set(int index, T element) {
            return source.set(mirror(index), element);
        }

        @Override
        public void add(int index, T element) {
            source.add(source.size() - index, element);
        }

        @Override
        public T remove(int index) {
            return source.remove(mirror(index));
        }

        @Override
        public int indexOf(Object o) {
            int index = source.lastIndexOf(o);
            return index < 0 ? -1 : mirror(index);
        }

        @Override
        public int lastIndexOf(Object o) {
            int index = source.indexOf(o);
            return index < 0 ? -1 : mirror(index);
        }

        @Override
        public java.util.List<T> subList(int fromIndex, int toIndex) {
            return java.util.List.copyOf(this).subList(fromIndex, toIndex);
        }

        @Override
        public Iterator<T> iterator() {
            return source.descendingIterator();
        }

        @Override
        public void clear() {
            source.clear();
        }
    }
}
