// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.AbstractList;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

public class NonNullList<E> extends AbstractList<E> {
    private final List<E> list;
    private final E defaultValue;

    public static <E> NonNullList<E> create() {
        return new NonNullList<>(new ArrayList<>(), null);
    }

    public static <E> NonNullList<E> createWithCapacity(int capacity) {
        return new NonNullList<>(new ArrayList<>(capacity), null);
    }

    public static <E> NonNullList<E> withSize(int size, E defaultValue) {
        Objects.requireNonNull(defaultValue);
        Object[] filled = new Object[size];
        Arrays.fill(filled, defaultValue);
        @SuppressWarnings("unchecked")
        List<E> backing = (List<E>) Arrays.asList(filled);
        return new NonNullList<>(backing, defaultValue);
    }

    @SafeVarargs
    public static <E> NonNullList<E> of(E defaultValue, E... values) {
        return new NonNullList<>(Arrays.asList(values), defaultValue);
    }

    protected NonNullList(List<E> list, E defaultValue) {
        this.list = list;
        this.defaultValue = defaultValue;
    }

    @Override
    public E get(int index) {
        return this.list.get(index);
    }

    @Override
    public E set(int index, E element) {
        Objects.requireNonNull(element);
        return this.list.set(index, element);
    }

    @Override
    public void add(int index, E element) {
        Objects.requireNonNull(element);
        this.list.add(index, element);
    }

    @Override
    public E remove(int index) {
        return this.list.remove(index);
    }

    @Override
    public int size() {
        return this.list.size();
    }

    @Override
    public void clear() {
        if (this.defaultValue == null) {
            super.clear();
        } else {
            for (int i = 0; i < this.size(); i++) {
                this.set(i, this.defaultValue);
            }
        }
    }
}
