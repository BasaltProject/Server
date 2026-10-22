package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

public final class ListTag extends AbstractList<Tag> implements CollectionTag {
    private final List<Tag> list = new ArrayList<>();

    public ListTag() {}

    public void addAndUnwrap(Tag tag) {
        list.add(tag);
    }

    @Override
    public Tag get(int index) {
        return list.get(index);
    }

    @Override
    public Tag set(int index, Tag tag) {
        return list.set(index, tag);
    }

    @Override
    public void add(int index, Tag tag) {
        list.add(index, tag);
    }

    @Override
    public Tag remove(int index) {
        return list.remove(index);
    }

    @Override
    public int size() {
        return list.size();
    }

    @Override
    public boolean isEmpty() {
        return list.isEmpty();
    }

    public Optional<String> getString(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof StringTag tag
            ? Optional.of(tag.value()) : Optional.empty();
    }

    public CompoundTag getCompoundOrEmpty(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof CompoundTag tag
            ? tag : new CompoundTag();
    }

    public Optional<CompoundTag> getCompound(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof CompoundTag tag
            ? Optional.of(tag) : Optional.empty();
    }

    public float getFloatOr(int index, float fallback) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? tag.floatValue() : fallback;
    }

    public double getDoubleOr(int index, double fallback) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? tag.doubleValue() : fallback;
    }

    public int getIntOr(int index, int fallback) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? tag.intValue() : fallback;
    }

    public ListTag getListOrEmpty(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof ListTag tag ? tag : new ListTag();
    }

    @Override
    public byte getId() {
        return TAG_LIST;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeByte(list.isEmpty() ? TAG_END : list.get(0).getId());
        output.writeInt(list.size());
        for (Tag tag : list) {
            tag.write(output);
        }
    }

    @Override
    public ListTag copy() {
        ListTag copy = new ListTag();
        for (Tag tag : list) {
            copy.list.add(tag.copy());
        }
        return copy;
    }

    @Override
    public String toString() {
        return list.toString();
    }
}
