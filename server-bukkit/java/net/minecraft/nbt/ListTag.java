// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.stream.Stream;

public final class ListTag extends AbstractList<Tag> implements CollectionTag {
    private final List<Tag> list;

    public ListTag() {
        this(new ArrayList<>());
    }

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
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<ListTag> TYPE = new TagType<>() {
        @Override
        public ListTag load(DataInput input, NbtAccounter accounter) throws IOException {
            accounter.pushDepth();
            try {
                accounter.accountBytes(37);
                byte elementId = input.readByte();
                int length = input.readInt();
                if (elementId == TAG_END && length > 0) {
                    throw new IOException("Missing type on ListTag");
                }
                accounter.accountBytes(4, length);
                TagType<?> elementType = TagTypes.getType(elementId);
                List<Tag> values = new ArrayList<>(Math.min(Math.max(length, 0), 4096));
                for (int i = 0; i < length; i++) {
                    values.add(elementType.load(input, accounter));
                }
                return new ListTag(values);
            } finally {
                accounter.popDepth();
            }
        }

        @Override
        public StreamTagVisitor.ValueResult parse(DataInput input, StreamTagVisitor output, NbtAccounter accounter)
            throws IOException {
            return load(input, accounter).accept(output);
        }

        @Override
        public void skip(DataInput input, int count, NbtAccounter accounter) throws IOException {
            for (int i = 0; i < count; i++) {
                skip(input, accounter);
            }
        }

        @Override
        public void skip(DataInput input, NbtAccounter accounter) throws IOException {
            load(input, accounter);
        }

        @Override
        public String getName() {
            return "LIST";
        }

        @Override
        public String getPrettyName() {
            return "TAG_List";
        }
    };
    ListTag(List<Tag> list) {
        this.list = list;
    }
    byte identifyRawElementType() {
        byte type = TAG_END;
        for (Tag tag : list) {
            if (type == TAG_END) {
                type = tag.getId();
            } else if (type != tag.getId()) {
                return 0;
            }
        }
        return type;
    }
    public int sizeInBytes() {
        int size = 37 + 4 * list.size();
        for (Tag tag : list) {
            size += tag.sizeInBytes();
        }
        return size;
    }
    public TagType<ListTag> getType() {
        return TYPE;
    }
    public Optional<ListTag> getList(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof ListTag tag
            ? Optional.of(tag) : Optional.empty();
    }
    public Optional<Short> getShort(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? Optional.of(tag.shortValue()) : Optional.empty();
    }
    public short getShortOr(int index, short defaultValue) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? tag.shortValue() : defaultValue;
    }
    public Optional<Integer> getInt(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? Optional.of(tag.intValue()) : Optional.empty();
    }
    public Optional<int[]> getIntArray(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof IntArrayTag tag
            ? Optional.of(tag.getAsIntArray()) : Optional.empty();
    }
    public Optional<long[]> getLongArray(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof LongArrayTag tag
            ? Optional.of(tag.getAsLongArray()) : Optional.empty();
    }
    public Optional<Double> getDouble(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? Optional.of(tag.doubleValue()) : Optional.empty();
    }
    public Optional<Float> getFloat(int index) {
        return index >= 0 && index < list.size() && list.get(index) instanceof NumericTag tag
            ? Optional.of(tag.floatValue()) : Optional.empty();
    }
    public String getStringOr(int index, String defaultValue) {
        return index >= 0 && index < list.size() && list.get(index) instanceof StringTag tag
            ? tag.value() : defaultValue;
    }
    public boolean setTag(int index, Tag tag) {
        if (index < 0 || index >= list.size() || tag.getId() == TAG_END) {
            return false;
        }
        list.set(index, tag);
        return true;
    }
    public boolean addTag(int index, Tag tag) {
        if (index < 0 || index > list.size() || tag.getId() == TAG_END) {
            return false;
        }
        list.add(index, tag);
        return true;
    }
    public Optional<ListTag> asList() {
        return Optional.of(this);
    }
    public boolean equals(Object obj) {
        return this == obj || obj instanceof ListTag other && list.equals(other.list);
    }
    public int hashCode() {
        return list.hashCode();
    }
    public Stream<Tag> stream() {
        return list.stream();
    }
    public Stream<CompoundTag> compoundStream() {
        return list.stream().filter(tag -> tag instanceof CompoundTag).map(tag -> (CompoundTag) tag);
    }
    public void accept(TagVisitor visitor) {
        visitor.visitList(this);
    }
    public void clear() {
        list.clear();
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        byte elementId = identifyRawElementType();
        switch (visitor.visitList(TagTypes.getType(elementId), list.size())) {
            case HALT:
                return StreamTagVisitor.ValueResult.HALT;
            case BREAK:
                return visitor.visitContainerEnd();
            default:
                break;
        }
        for (int i = 0; i < list.size(); i++) {
            Tag tag = list.get(i);
            switch (visitor.visitElement(tag.getType(), i)) {
                case HALT:
                    return StreamTagVisitor.ValueResult.HALT;
                case BREAK:
                    return visitor.visitContainerEnd();
                case SKIP:
                    break;
                default:
                    switch (tag.accept(visitor)) {
                        case HALT:
                            return StreamTagVisitor.ValueResult.HALT;
                        case BREAK:
                            return visitor.visitContainerEnd();
                        default:
                            break;
                    }
            }
        }
        return visitor.visitContainerEnd();
    }
}
