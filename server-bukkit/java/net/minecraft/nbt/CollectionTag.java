// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.util.Iterator;
import java.util.stream.StreamSupport;
import java.util.stream.Stream;

public interface CollectionTag extends Tag, Iterable<Tag> {
    int size();

    boolean isEmpty();
    void clear();
    boolean setTag(int index, Tag tag);
    boolean addTag(int index, Tag tag);
    Tag remove(int index);
    Tag get(int index);
    default Iterator<Tag> iterator() {
        return new Iterator<>() {
            private int index;

            @Override
            public boolean hasNext() {
        return index < size();
            }

            @Override
            public Tag next() {
        if (!hasNext()) {
            throw new java.util.NoSuchElementException();
        }
        return get(index++);
            }
        };
    }
    default Stream<Tag> stream() {
        return StreamSupport.stream(spliterator(), false);
    }
}
