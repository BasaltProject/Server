// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import java.util.Collection;
import java.util.function.BiConsumer;

public final class CompoundTag implements Tag {
    public static final com.mojang.serialization.Codec<CompoundTag> CODEC =
        com.mojang.serialization.Codec.PASSTHROUGH.comapFlatMap(dynamic -> {
            Tag converted = dynamic.convert(NbtOps.INSTANCE).getValue();
            return converted instanceof CompoundTag compound
                ? com.mojang.serialization.DataResult.success(compound)
                : com.mojang.serialization.DataResult.error(() -> "Not a compound tag: " + converted);
        }, compound -> new com.mojang.serialization.Dynamic<>(NbtOps.INSTANCE, compound));

    private final Map<String, Tag> tags;

    public CompoundTag() {
        this(new LinkedHashMap<>());
    }

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
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<CompoundTag> TYPE = new TagType<>() {
        @Override
        public CompoundTag load(DataInput input, NbtAccounter accounter) throws IOException {
            accounter.pushDepth();
            try {
                accounter.accountBytes(48);
                CompoundTag result = new CompoundTag();
                byte id;
                while ((id = input.readByte()) != TAG_END) {
                    String key = input.readUTF();
                    accounter.accountBytes(28 + 2 * key.length());
                    accounter.accountBytes(36);
                    result.tags.put(key, TagTypes.getType(id).load(input, accounter));
                }
                return result;
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
            return "COMPOUND";
        }

        @Override
        public String getPrettyName() {
            return "TAG_Compound";
        }
    };
    CompoundTag(Map<String, Tag> tags) {
        this.tags = tags;
    }
    public int sizeInBytes() {
        int size = 48;
        for (Map.Entry<String, Tag> entry : tags.entrySet()) {
            size += 28 + 2 * entry.getKey().length() + 36 + entry.getValue().sizeInBytes();
        }
        return size;
    }
    public Collection<Tag> values() {
        return tags.values();
    }
    public void forEach(BiConsumer<String, Tag> consumer) {
        tags.forEach(consumer);
    }
    public TagType<CompoundTag> getType() {
        return TYPE;
    }
    CompoundTag shallowCopy() {
        return new CompoundTag(new LinkedHashMap<>(tags));
    }
    public Optional<CompoundTag> asCompound() {
        return Optional.of(this);
    }
    public void accept(TagVisitor visitor) {
        visitor.visitCompound(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        for (Map.Entry<String, Tag> entry : tags.entrySet()) {
            Tag tag = entry.getValue();
            StreamTagVisitor.EntryResult entryResult = visitor.visitEntry(tag.getType(), entry.getKey());
            switch (entryResult) {
                case HALT:
                    return StreamTagVisitor.ValueResult.HALT;
                case BREAK:
                    return visitor.visitContainerEnd();
                case SKIP:
                    break;
                default:
                    StreamTagVisitor.ValueResult result = tag.accept(visitor);
                    switch (result) {
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
    public <T> void store(String name, Codec<T> codec, T value) {
        store(name, codec, NbtOps.INSTANCE, value);
    }
    public <T> void storeNullable(String name, Codec<T> codec, T value) {
        storeNullable(name, codec, NbtOps.INSTANCE, value);
    }
    public <T> void store(String name, Codec<T> codec, DynamicOps<Tag> ops, T value) {
        tags.put(name, codec.encodeStart(ops, value).getOrThrow());
    }
    public <T> void storeNullable(String name, Codec<T> codec, DynamicOps<Tag> ops, T value) {
        if (value != null) {
            store(name, codec, ops, value);
        }
    }
    public <T> void store(MapCodec<T> codec, T value) {
        store(codec, NbtOps.INSTANCE, value);
    }
    public <T> void store(MapCodec<T> codec, DynamicOps<Tag> ops, T value) {
        Tag encoded = codec.encoder().encodeStart(ops, value).getOrThrow();
        if (encoded instanceof CompoundTag compound) {
            merge(compound);
        }
    }
    public <T> Optional<T> read(String name, Codec<T> codec) {
        return read(name, codec, NbtOps.INSTANCE);
    }
    public <T> Optional<T> read(String name, Codec<T> codec, DynamicOps<Tag> ops) {
        Tag tag = tags.get(name);
        return tag == null ? Optional.empty() : codec.parse(ops, tag).result();
    }
    public <T> Optional<T> read(MapCodec<T> codec) {
        return read(codec, NbtOps.INSTANCE);
    }
    public <T> Optional<T> read(MapCodec<T> codec, DynamicOps<Tag> ops) {
        return codec.decoder().parse(ops, this).result();
    }
}
