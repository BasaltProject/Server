// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Optional;

public interface Tag {
    byte TAG_END = 0;
    byte TAG_BYTE = 1;
    byte TAG_SHORT = 2;
    byte TAG_INT = 3;
    byte TAG_LONG = 4;
    byte TAG_FLOAT = 5;
    byte TAG_DOUBLE = 6;
    byte TAG_BYTE_ARRAY = 7;
    byte TAG_STRING = 8;
    byte TAG_LIST = 9;
    byte TAG_COMPOUND = 10;
    byte TAG_INT_ARRAY = 11;
    byte TAG_LONG_ARRAY = 12;

    byte getId();

    void write(DataOutput output) throws IOException;

    Tag copy();
    int OBJECT_HEADER = 8;
    int ARRAY_HEADER = 4;
    int OBJECT_REFERENCE = 4;
    int STRING_SIZE = 28;
    int MAX_DEPTH = 512;
    default TagType<?> getType() {
        return TagTypes.getType(getId());
    }
    default int sizeInBytes() {
        return OBJECT_HEADER;
    }
    void accept(TagVisitor visitor);
    StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor);
    default void acceptAsRoot(StreamTagVisitor output) {
        if (output.visitRootEntry(getType()) == StreamTagVisitor.ValueResult.CONTINUE) {
            accept(output);
        }
    }
    default Optional<String> asString() {
        return Optional.empty();
    }
    default Optional<Number> asNumber() {
        return Optional.empty();
    }
    default Optional<Byte> asByte() {
        return Optional.empty();
    }
    default Optional<Short> asShort() {
        return Optional.empty();
    }
    default Optional<Integer> asInt() {
        return Optional.empty();
    }
    default Optional<Long> asLong() {
        return Optional.empty();
    }
    default Optional<Float> asFloat() {
        return Optional.empty();
    }
    default Optional<Double> asDouble() {
        return Optional.empty();
    }
    default Optional<Boolean> asBoolean() {
        return Optional.empty();
    }
    default Optional<byte[]> asByteArray() {
        return Optional.empty();
    }
    default Optional<int[]> asIntArray() {
        return Optional.empty();
    }
    default Optional<long[]> asLongArray() {
        return Optional.empty();
    }
    default Optional<CompoundTag> asCompound() {
        return Optional.empty();
    }
    default Optional<ListTag> asList() {
        return Optional.empty();
    }
}
