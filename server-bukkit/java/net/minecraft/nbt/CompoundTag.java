package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

public final class CompoundTag implements Tag {
    public static final com.mojang.serialization.Codec<CompoundTag> CODEC =
        com.mojang.serialization.Codec.PASSTHROUGH.comapFlatMap(dynamic -> {
            Tag converted = dynamic.convert(NbtOps.INSTANCE).getValue();
            return converted instanceof CompoundTag compound
                ? com.mojang.serialization.DataResult.success(compound)
                : com.mojang.serialization.DataResult.error(() -> "Not a compound tag: " + converted);
        }, compound -> new com.mojang.serialization.Dynamic<>(NbtOps.INSTANCE, compound));

    private final Map<String, Tag> tags = new LinkedHashMap<>();

    public CompoundTag() {}

    public Set<String> keySet() {
        return tags.keySet();
    }

    public Set<Map.Entry<String, Tag>> entrySet() {
        return tags.entrySet();
    }

    public int size() {
        return tags.size();
    }

    public boolean isEmpty() {
        return tags.isEmpty();
    }

    public Tag put(String name, Tag tag) {
        return tags.put(name, tag);
    }

    public Tag get(String name) {
        return tags.get(name);
    }

    public boolean contains(String name) {
        return tags.containsKey(name);
    }

    public Tag remove(String name) {
        return tags.remove(name);
    }

    public void putBoolean(String name, boolean value) {
        tags.put(name, ByteTag.valueOf((byte) (value ? 1 : 0)));
    }

    public void putString(String name, String value) {
        tags.put(name, StringTag.valueOf(value));
    }

    public void putByteArray(String name, byte[] value) {
        tags.put(name, new ByteArrayTag(value));
    }

    public void putIntArray(String name, int[] value) {
        tags.put(name, new IntArrayTag(value));
    }

    public void putLongArray(String name, long[] value) {
        tags.put(name, new LongArrayTag(value));
    }

    public void putByte(String name, byte value) {
        tags.put(name, ByteTag.valueOf(value));
    }

    public Optional<Byte> getByte(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.byteValue()) : Optional.empty();
    }

    public byte getByteOr(String name, byte fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.byteValue() : fallback;
    }

    public void putShort(String name, short value) {
        tags.put(name, ShortTag.valueOf(value));
    }

    public Optional<Short> getShort(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.shortValue()) : Optional.empty();
    }

    public short getShortOr(String name, short fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.shortValue() : fallback;
    }

    public void putInt(String name, int value) {
        tags.put(name, IntTag.valueOf(value));
    }

    public Optional<Integer> getInt(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.intValue()) : Optional.empty();
    }

    public int getIntOr(String name, int fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.intValue() : fallback;
    }

    public void putLong(String name, long value) {
        tags.put(name, LongTag.valueOf(value));
    }

    public Optional<Long> getLong(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.longValue()) : Optional.empty();
    }

    public long getLongOr(String name, long fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.longValue() : fallback;
    }

    public void putFloat(String name, float value) {
        tags.put(name, FloatTag.valueOf(value));
    }

    public Optional<Float> getFloat(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.floatValue()) : Optional.empty();
    }

    public float getFloatOr(String name, float fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.floatValue() : fallback;
    }

    public void putDouble(String name, double value) {
        tags.put(name, DoubleTag.valueOf(value));
    }

    public Optional<Double> getDouble(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.doubleValue()) : Optional.empty();
    }

    public double getDoubleOr(String name, double fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.doubleValue() : fallback;
    }

    public Optional<Boolean> getBoolean(String name) {
        return tags.get(name) instanceof NumericTag tag ? Optional.of(tag.byteValue() != 0) : Optional.empty();
    }

    public boolean getBooleanOr(String name, boolean fallback) {
        return tags.get(name) instanceof NumericTag tag ? tag.byteValue() != 0 : fallback;
    }

    public Optional<String> getString(String name) {
        return tags.get(name) instanceof StringTag tag ? Optional.of(tag.value()) : Optional.empty();
    }

    public String getStringOr(String name, String fallback) {
        return tags.get(name) instanceof StringTag tag ? tag.value() : fallback;
    }

    public Optional<byte[]> getByteArray(String name) {
        return tags.get(name) instanceof ByteArrayTag tag ? Optional.of(tag.getAsByteArray()) : Optional.empty();
    }

    public Optional<int[]> getIntArray(String name) {
        return tags.get(name) instanceof IntArrayTag tag ? Optional.of(tag.getAsIntArray()) : Optional.empty();
    }

    public Optional<long[]> getLongArray(String name) {
        return tags.get(name) instanceof LongArrayTag tag ? Optional.of(tag.getAsLongArray()) : Optional.empty();
    }

    public Optional<CompoundTag> getCompound(String name) {
        return tags.get(name) instanceof CompoundTag tag ? Optional.of(tag) : Optional.empty();
    }

    public CompoundTag getCompoundOrEmpty(String name) {
        return tags.get(name) instanceof CompoundTag tag ? tag : new CompoundTag();
    }

    public Optional<ListTag> getList(String name) {
        return tags.get(name) instanceof ListTag tag ? Optional.of(tag) : Optional.empty();
    }

    public ListTag getListOrEmpty(String name) {
        return tags.get(name) instanceof ListTag tag ? tag : new ListTag();
    }

    public CompoundTag merge(CompoundTag other) {
        for (Map.Entry<String, Tag> entry : other.tags.entrySet()) {
            Tag value = entry.getValue();
            if (value instanceof CompoundTag compound && tags.get(entry.getKey()) instanceof CompoundTag mine) {
                mine.merge(compound);
            } else {
                tags.put(entry.getKey(), value.copy());
            }
        }
        return this;
    }

    @Override
    public byte getId() {
        return TAG_COMPOUND;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        for (Map.Entry<String, Tag> entry : tags.entrySet()) {
            output.writeByte(entry.getValue().getId());
            output.writeUTF(entry.getKey());
            entry.getValue().write(output);
        }
        output.writeByte(TAG_END);
    }

    @Override
    public CompoundTag copy() {
        CompoundTag copy = new CompoundTag();
        for (Map.Entry<String, Tag> entry : tags.entrySet()) {
            copy.tags.put(entry.getKey(), entry.getValue().copy());
        }
        return copy;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof CompoundTag tag && tag.tags.equals(tags);
    }

    @Override
    public int hashCode() {
        return tags.hashCode();
    }

    @Override
    public String toString() {
        StringBuilder text = new StringBuilder("{");
        for (Map.Entry<String, Tag> entry : tags.entrySet()) {
            if (text.length() > 1) {
                text.append(',');
            }
            text.append(entry.getKey()).append(':').append(entry.getValue());
        }
        return text.append('}').toString();
    }
}
