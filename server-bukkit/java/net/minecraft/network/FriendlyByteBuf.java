// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import com.mojang.serialization.JsonOps;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufInputStream;
import io.netty.buffer.ByteBufOutputStream;
import io.netty.handler.codec.DecoderException;
import io.netty.handler.codec.EncoderException;
import java.nio.charset.StandardCharsets;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.EndTag;
import net.minecraft.nbt.NbtIo;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import io.netty.buffer.ByteBufAllocator;
import io.netty.util.ByteProcessor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.channels.GatheringByteChannel;
import java.nio.channels.ScatteringByteChannel;
import java.nio.charset.Charset;
import java.util.BitSet;
import java.util.EnumSet;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.function.IntFunction;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Registry;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.Tag;
import net.minecraft.network.codec.StreamDecoder;
import net.minecraft.network.codec.StreamEncoder;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import org.joml.Quaternionf;
import org.joml.Quaternionfc;
import org.joml.Vector3f;
import org.joml.Vector3fc;

/** Present so plugins that probe the vanilla buffer class by reflection find what they look for. */
public class FriendlyByteBuf extends ByteBuf {
    private final ByteBuf source;

    public FriendlyByteBuf(ByteBuf source) {
        this.source = source;
    }

    public ByteBuf source() {
        return source;
    }
    private static int readVarInt(ByteBuf input) {
        int result = 0;
        int shift = 0;
        byte current;
        do {
            current = input.readByte();
            result |= (current & 127) << shift * 7;
            if (++shift > 5) {
                throw new RuntimeException("VarInt too big");
            }
        } while ((current & 128) == 128);
        return result;
    }

    private static void writeVarInt(ByteBuf output, int value) {
        while ((value & -128) != 0) {
            output.writeByte(value & 127 | 128);
            value >>>= 7;
        }
        output.writeByte(value);
    }

    public static final short MAX_STRING_LENGTH = 32767;
    public static final int MAX_COMPONENT_STRING_LENGTH = 262144;
    public <T> T readWithCodecTrusted(DynamicOps<Tag> ops, Codec<T> codec) {
        return readWithCodec(ops, codec, NbtAccounter.unlimitedHeap());
    }
    public <T> T readWithCodec(DynamicOps<Tag> ops, Codec<T> codec, NbtAccounter accounter) {
        Tag tag = readNbt(accounter);
        return codec.parse(ops, tag).getOrThrow(message -> new DecoderException("Failed to decode: " + message + " " + tag));
    }
    public <T> FriendlyByteBuf writeWithCodec(DynamicOps<Tag> ops, Codec<T> codec, T value) {
        Tag tag = codec.encodeStart(ops, value).getOrThrow(message -> new EncoderException("Failed to encode: " + message + " " + value));
        writeNbt(tag);
        return this;
    }
    public <T> T readLenientJsonWithCodec(Codec<T> codec) {
        JsonElement json = JsonParser.parseString(readUtf());
        return codec.parse(JsonOps.INSTANCE, json).getOrThrow(message -> new DecoderException("Failed to decode JSON: " + message));
    }
    public <T> void writeJsonWithCodec(Codec<T> codec, T value) {
        JsonElement json = codec.encodeStart(JsonOps.INSTANCE, value).getOrThrow(message -> new EncoderException("Failed to encode: " + message + " " + value));
        writeUtf(new Gson().toJson(json));
    }
    public void readWithCount(Consumer<FriendlyByteBuf> reader) {
        int count = readVarInt();
        for (int i = 0; i < count; i++) {
            reader.accept(this);
        }
    }
    public <E extends Enum<E>> void writeEnumSet(EnumSet<E> set, Class<E> clazz) {
        E[] universe = clazz.getEnumConstants();
        BitSet bits = new BitSet(universe.length);
        for (int i = 0; i < universe.length; i++) {
            bits.set(i, set.contains(universe[i]));
        }
        writeFixedBitSet(bits, universe.length);
    }
    public <E extends Enum<E>> EnumSet<E> readEnumSet(Class<E> clazz) {
        E[] universe = clazz.getEnumConstants();
        BitSet bits = readFixedBitSet(universe.length);
        EnumSet<E> result = EnumSet.noneOf(clazz);
        for (int i = 0; i < universe.length; i++) {
            if (bits.get(i)) {
        result.add(universe[i]);
            }
        }
        return result;
    }
    public <T> void writeOptional(Optional<T> value, StreamEncoder<? super FriendlyByteBuf, T> valueWriter) {
        writeBoolean(value.isPresent());
        value.ifPresent(present -> valueWriter.encode(this, present));
    }
    public <T> Optional<T> readOptional(StreamDecoder<? super FriendlyByteBuf, T> valueReader) {
        return readBoolean() ? Optional.of(valueReader.decode(this)) : Optional.empty();
    }
    public <L, R> void writeEither(Either<L, R> value, StreamEncoder<? super FriendlyByteBuf, L> leftWriter, StreamEncoder<? super FriendlyByteBuf, R> rightWriter) {
        if (value.left().isPresent()) {
            writeBoolean(true);
            leftWriter.encode(this, value.left().get());
        } else {
            writeBoolean(false);
            rightWriter.encode(this, value.right().get());
        }
    }
    public <L, R> Either<L, R> readEither(StreamDecoder<? super FriendlyByteBuf, L> leftReader, StreamDecoder<? super FriendlyByteBuf, R> rightReader) {
        return readBoolean() ? Either.left(leftReader.decode(this)) : Either.right(rightReader.decode(this));
    }
    public <T> T readNullable(StreamDecoder<? super FriendlyByteBuf, T> valueDecoder) {
        return readNullable(this, valueDecoder);
    }
    public static <T, B extends ByteBuf> T readNullable(B input, StreamDecoder<? super B, T> valueDecoder) {
        return input.readBoolean() ? valueDecoder.decode(input) : null;
    }
    public <T> void writeNullable(T value, StreamEncoder<? super FriendlyByteBuf, T> valueEncoder) {
        writeNullable(this, value, valueEncoder);
    }
    public static <T, B extends ByteBuf> void writeNullable(B output, T value, StreamEncoder<? super B, T> valueEncoder) {
        if (value != null) {
            output.writeBoolean(true);
            valueEncoder.encode(output, value);
        } else {
            output.writeBoolean(false);
        }
    }
    public byte[] readByteArray() {
        return readByteArray(this);
    }
    public static byte[] readByteArray(ByteBuf input) {
        return readByteArray(input, input.readableBytes());
    }
    public FriendlyByteBuf writeByteArray(byte[] bytes) {
        writeByteArray(this, bytes);
        return this;
    }
    public static void writeByteArray(ByteBuf output, byte[] bytes) {
        writeVarInt(output, bytes.length);
        output.writeBytes(bytes);
    }
    public byte[] readByteArray(int maxSize) {
        return readByteArray(this, maxSize);
    }
    public static byte[] readByteArray(ByteBuf input, int maxSize) {
        int length = readVarInt(input);
        if (length > maxSize) {
            throw new DecoderException("ByteArray with size " + length + " is bigger than allowed " + maxSize);
        }
        byte[] bytes = new byte[length];
        input.readBytes(bytes);
        return bytes;
    }
    public FriendlyByteBuf writeVarIntArray(int[] ints) {
        writeVarInt(ints.length);
        for (int value : ints) {
            writeVarInt(value);
        }
        return this;
    }
    public int[] readVarIntArray() {
        return readVarIntArray(readableBytes());
    }
    public int[] readVarIntArray(int maxSize) {
        int length = readVarInt();
        if (length > maxSize) {
            throw new DecoderException("VarIntArray with size " + length + " is bigger than allowed " + maxSize);
        }
        int[] ints = new int[length];
        for (int i = 0; i < length; i++) {
            ints[i] = readVarInt();
        }
        return ints;
    }
    public FriendlyByteBuf writeLongArray(long[] longs) {
        writeLongArray(this, longs);
        return this;
    }
    public static void writeLongArray(ByteBuf output, long[] longs) {
        writeVarInt(output, longs.length);
        for (long value : longs) {
            output.writeLong(value);
        }
    }
    public FriendlyByteBuf writeFixedSizeLongArray(long[] longs) {
        writeFixedSizeLongArray(this, longs);
        return this;
    }
    public static void writeFixedSizeLongArray(ByteBuf output, long[] longs) {
        for (long value : longs) {
            output.writeLong(value);
        }
    }
    public long[] readLongArray() {
        return readLongArray(this);
    }
    public long[] readFixedSizeLongArray(long[] output) {
        return readFixedSizeLongArray(this, output);
    }
    public static long[] readLongArray(ByteBuf input) {
        int length = readVarInt(input);
        int maxLength = input.readableBytes() / 8;
        if (length > maxLength) {
            throw new DecoderException("LongArray with size " + length + " is bigger than allowed " + maxLength);
        }
        return readFixedSizeLongArray(input, new long[length]);
    }
    public static long[] readFixedSizeLongArray(ByteBuf input, long[] output) {
        for (int i = 0; i < output.length; i++) {
            output[i] = input.readLong();
        }
        return output;
    }
    public BlockPos readBlockPos() {
        return readBlockPos(this);
    }
    public static BlockPos readBlockPos(ByteBuf input) {
        return BlockPos.of(input.readLong());
    }
    public FriendlyByteBuf writeBlockPos(BlockPos pos) {
        writeBlockPos(this, pos);
        return this;
    }
    public static void writeBlockPos(ByteBuf output, BlockPos pos) {
        output.writeLong(pos.asLong());
    }
    public ChunkPos readChunkPos() {
        return readChunkPos(this);
    }
    public FriendlyByteBuf writeChunkPos(ChunkPos pos) {
        writeChunkPos(this, pos);
        return this;
    }
    public static ChunkPos readChunkPos(ByteBuf input) {
        return ChunkPos.unpack(input.readLong());
    }
    public static void writeChunkPos(ByteBuf output, ChunkPos chunkPos) {
        output.writeLong(chunkPos.pack());
    }
    public GlobalPos readGlobalPos() {
        ResourceKey<net.minecraft.world.level.Level> dimension = readResourceKey(Registries.DIMENSION);
        return GlobalPos.of(dimension, readBlockPos());
    }
    public void writeGlobalPos(GlobalPos globalPos) {
        writeResourceKey(globalPos.dimension());
        writeBlockPos(globalPos.pos());
    }
    public Vector3f readVector3f() {
        return readVector3f(this);
    }
    public static Vector3f readVector3f(ByteBuf input) {
        return new Vector3f(input.readFloat(), input.readFloat(), input.readFloat());
    }
    public void writeVector3f(Vector3f v) {
        writeVector3f(this, v);
    }
    public static void writeVector3f(ByteBuf output, Vector3fc v) {
        output.writeFloat(v.x());
        output.writeFloat(v.y());
        output.writeFloat(v.z());
    }
    public Quaternionf readQuaternion() {
        return readQuaternion(this);
    }
    public static Quaternionf readQuaternion(ByteBuf input) {
        return new Quaternionf(input.readFloat(), input.readFloat(), input.readFloat(), input.readFloat());
    }
    public void writeQuaternion(Quaternionf q) {
        writeQuaternion(this, q);
    }
    public static void writeQuaternion(ByteBuf output, Quaternionfc value) {
        output.writeFloat(value.x());
        output.writeFloat(value.y());
        output.writeFloat(value.z());
        output.writeFloat(value.w());
    }
    public <T extends Enum<T>> T readEnum(Class<T> clazz) {
        return clazz.getEnumConstants()[readVarInt()];
    }
    public FriendlyByteBuf writeEnum(Enum<?> value) {
        return writeVarInt(value.ordinal());
    }
    public <T> T readById(IntFunction<T> converter) {
        return converter.apply(readVarInt());
    }
    public <T> FriendlyByteBuf writeById(ToIntFunction<T> converter, T value) {
        return writeVarInt(converter.applyAsInt(value));
    }
    public int readVarInt() {
        return readVarInt(this);
    }
    public long readVarLong() {
        long result = 0;
        int shift = 0;
        byte current;
        do {
            current = readByte();
            result |= (long) (current & 127) << shift;
            shift += 7;
            if (shift > 63) {
        throw new RuntimeException("VarLong too big");
            }
        } while ((current & 128) == 128);
        return result;
    }
    public FriendlyByteBuf writeUUID(UUID uuid) {
        writeUUID(this, uuid);
        return this;
    }
    public static void writeUUID(ByteBuf output, UUID uuid) {
        output.writeLong(uuid.getMostSignificantBits());
        output.writeLong(uuid.getLeastSignificantBits());
    }
    public UUID readUUID() {
        return readUUID(this);
    }
    public static UUID readUUID(ByteBuf input) {
        return new UUID(input.readLong(), input.readLong());
    }
    public FriendlyByteBuf writeVarInt(int value) {
        writeVarInt(this, value);
        return this;
    }
    public FriendlyByteBuf writeVarLong(long value) {
        while ((value & -128L) != 0L) {
            writeByte((int) (value & 127L) | 128);
            value >>>= 7;
        }
        writeByte((int) value);
        return this;
    }
    public FriendlyByteBuf writeNbt(Tag tag) {
        writeNbt(this, tag);
        return this;
    }
    public static void writeNbt(ByteBuf output, Tag tag) {
        try {
            NbtIo.writeAnyTag(tag == null ? EndTag.INSTANCE : tag, new ByteBufOutputStream(output));
        } catch (IOException e) {
            throw new EncoderException(e);
        }
    }
    public CompoundTag readNbt() {
        return readNbt((ByteBuf) this);
    }
    public static CompoundTag readNbt(ByteBuf input) {
        Tag result = readNbt(input, NbtAccounter.defaultQuota());
        if (result != null && !(result instanceof CompoundTag)) {
            throw new DecoderException("Not a compound tag: " + result);
        }
        return (CompoundTag) result;
    }
    public static Tag readNbt(ByteBuf input, NbtAccounter accounter) {
        try {
            Tag tag = NbtIo.readAnyTag(new ByteBufInputStream(input), accounter);
            return tag.getId() == Tag.TAG_END ? null : tag;
        } catch (IOException e) {
            throw new EncoderException(e);
        }
    }
    public Tag readNbt(NbtAccounter accounter) {
        return readNbt(this, accounter);
    }
    public String readUtf() {
        return readUtf(MAX_STRING_LENGTH);
    }
    public String readUtf(int maxLength) {
        int maxBytes = maxLength * 4;
        int length = readVarInt();
        if (length > maxBytes) {
            throw new DecoderException("The received encoded string buffer length is longer than maximum allowed (" + length + " > " + maxBytes + ")");
        }
        if (length < 0) {
            throw new DecoderException("The received encoded string buffer length is less than zero! Weird string!");
        }
        String text = readString(length, StandardCharsets.UTF_8);
        if (text.length() > maxLength) {
            throw new DecoderException("The received string length is longer than maximum allowed (" + text.length() + " > " + maxLength + ")");
        }
        return text;
    }
    public FriendlyByteBuf writeUtf(String value) {
        return writeUtf(value, MAX_STRING_LENGTH);
    }
    public FriendlyByteBuf writeUtf(String value, int maxLength) {
        if (value.length() > maxLength) {
            throw new EncoderException("String too big (was " + value.length() + " characters, max " + maxLength + ")");
        }
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        if (bytes.length > maxLength * 3) {
            throw new EncoderException("String too big (was " + bytes.length + " bytes encoded, max " + maxLength * 3 + ")");
        }
        writeVarInt(bytes.length);
        writeBytes(bytes);
        return this;
    }
    public Identifier readIdentifier() {
        return Identifier.parse(readUtf(MAX_STRING_LENGTH));
    }
    public FriendlyByteBuf writeIdentifier(Identifier identifier) {
        return writeUtf(identifier.toString());
    }
    public <T> ResourceKey<T> readResourceKey(ResourceKey<? extends Registry<T>> registry) {
        return ResourceKey.create(registry, readIdentifier());
    }
    public void writeResourceKey(ResourceKey<?> key) {
        writeIdentifier(key.identifier());
    }
    public <T> ResourceKey<? extends Registry<T>> readRegistryKey() {
        return ResourceKey.createRegistryKey(readIdentifier());
    }
    public BitSet readBitSet() {
        return BitSet.valueOf(readLongArray());
    }
    public void writeBitSet(BitSet bitSet) {
        writeLongArray(bitSet.toLongArray());
    }
    public BitSet readFixedBitSet(int size) {
        return readFixedBitSet(this, size);
    }
    public void writeFixedBitSet(BitSet bitSet, int size) {
        writeFixedBitSet(this, bitSet, size);
    }
    public static BitSet readFixedBitSet(ByteBuf input, int size) {
        byte[] bytes = new byte[(size + 7) / 8];
        input.readBytes(bytes);
        return BitSet.valueOf(bytes);
    }
    public static void writeFixedBitSet(ByteBuf output, BitSet bitSet, int size) {
        if (bitSet.length() > size) {
            throw new EncoderException("BitSet is larger than expected size (" + bitSet.length() + ">" + size + ")");
        }
        byte[] bytes = bitSet.toByteArray();
        output.writeBytes(java.util.Arrays.copyOf(bytes, (size + 7) / 8));
    }
    public static int readContainerId(ByteBuf input) {
        return readVarInt(input);
    }
    public int readContainerId() {
        return readContainerId(this);
    }
    public static void writeContainerId(ByteBuf output, int id) {
        writeVarInt(output, id);
    }
    public void writeContainerId(int id) {
        writeContainerId(this, id);
    }
    public boolean isContiguous() {
        return source.isContiguous();
    }
    public int maxFastWritableBytes() {
        return source.maxFastWritableBytes();
    }
    public int capacity() {
        return source.capacity();
    }
    public FriendlyByteBuf capacity(int newCapacity) {
        source.capacity(newCapacity);
        return this;
    }
    public int maxCapacity() {
        return source.maxCapacity();
    }
    public ByteBufAllocator alloc() {
        return source.alloc();
    }
    public ByteOrder order() {
        return source.order();
    }
    public ByteBuf order(ByteOrder endianness) {
        return new FriendlyByteBuf(source.order(endianness));
    }
    public ByteBuf unwrap() {
        return source;
    }
    public boolean isDirect() {
        return source.isDirect();
    }
    public boolean isReadOnly() {
        return source.isReadOnly();
    }
    public ByteBuf asReadOnly() {
        return new FriendlyByteBuf(source.asReadOnly());
    }
    public int readerIndex() {
        return source.readerIndex();
    }
    public FriendlyByteBuf readerIndex(int readerIndex) {
        source.readerIndex(readerIndex);
        return this;
    }
    public int writerIndex() {
        return source.writerIndex();
    }
    public FriendlyByteBuf writerIndex(int writerIndex) {
        source.writerIndex(writerIndex);
        return this;
    }
    public FriendlyByteBuf setIndex(int readerIndex, int writerIndex) {
        source.setIndex(readerIndex, writerIndex);
        return this;
    }
    public int readableBytes() {
        return source.readableBytes();
    }
    public int writableBytes() {
        return source.writableBytes();
    }
    public int maxWritableBytes() {
        return source.maxWritableBytes();
    }
    public boolean isReadable() {
        return source.isReadable();
    }
    public boolean isReadable(int size) {
        return source.isReadable(size);
    }
    public boolean isWritable() {
        return source.isWritable();
    }
    public boolean isWritable(int size) {
        return source.isWritable(size);
    }
    public FriendlyByteBuf clear() {
        source.clear();
        return this;
    }
    public FriendlyByteBuf markReaderIndex() {
        source.markReaderIndex();
        return this;
    }
    public FriendlyByteBuf resetReaderIndex() {
        source.resetReaderIndex();
        return this;
    }
    public FriendlyByteBuf markWriterIndex() {
        source.markWriterIndex();
        return this;
    }
    public FriendlyByteBuf resetWriterIndex() {
        source.resetWriterIndex();
        return this;
    }
    public FriendlyByteBuf discardReadBytes() {
        source.discardReadBytes();
        return this;
    }
    public FriendlyByteBuf discardSomeReadBytes() {
        source.discardSomeReadBytes();
        return this;
    }
    public FriendlyByteBuf ensureWritable(int minWritableBytes) {
        source.ensureWritable(minWritableBytes);
        return this;
    }
    public int ensureWritable(int minWritableBytes, boolean force) {
        return source.ensureWritable(minWritableBytes, force);
    }
    public boolean getBoolean(int index) {
        return source.getBoolean(index);
    }
    public byte getByte(int index) {
        return source.getByte(index);
    }
    public short getUnsignedByte(int index) {
        return source.getUnsignedByte(index);
    }
    public short getShort(int index) {
        return source.getShort(index);
    }
    public short getShortLE(int index) {
        return source.getShortLE(index);
    }
    public int getUnsignedShort(int index) {
        return source.getUnsignedShort(index);
    }
    public int getUnsignedShortLE(int index) {
        return source.getUnsignedShortLE(index);
    }
    public int getMedium(int index) {
        return source.getMedium(index);
    }
    public int getMediumLE(int index) {
        return source.getMediumLE(index);
    }
    public int getUnsignedMedium(int index) {
        return source.getUnsignedMedium(index);
    }
    public int getUnsignedMediumLE(int index) {
        return source.getUnsignedMediumLE(index);
    }
    public int getInt(int index) {
        return source.getInt(index);
    }
    public int getIntLE(int index) {
        return source.getIntLE(index);
    }
    public long getUnsignedInt(int index) {
        return source.getUnsignedInt(index);
    }
    public long getUnsignedIntLE(int index) {
        return source.getUnsignedIntLE(index);
    }
    public long getLong(int index) {
        return source.getLong(index);
    }
    public long getLongLE(int index) {
        return source.getLongLE(index);
    }
    public char getChar(int index) {
        return source.getChar(index);
    }
    public float getFloat(int index) {
        return source.getFloat(index);
    }
    public double getDouble(int index) {
        return source.getDouble(index);
    }
    public FriendlyByteBuf getBytes(int index, ByteBuf dst) {
        source.getBytes(index, dst);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, ByteBuf dst, int length) {
        source.getBytes(index, dst, length);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, ByteBuf dst, int dstIndex, int length) {
        source.getBytes(index, dst, dstIndex, length);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, byte[] dst) {
        source.getBytes(index, dst);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, byte[] dst, int dstIndex, int length) {
        source.getBytes(index, dst, dstIndex, length);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, ByteBuffer dst) {
        source.getBytes(index, dst);
        return this;
    }
    public FriendlyByteBuf getBytes(int index, OutputStream out, int length) throws IOException {
        source.getBytes(index, out, length);
        return this;
    }
    public int getBytes(int index, GatheringByteChannel out, int length) throws IOException {
        return source.getBytes(index, out, length);
    }
    public int getBytes(int index, FileChannel out, long position, int length) throws IOException {
        return source.getBytes(index, out, position, length);
    }
    public CharSequence getCharSequence(int index, int length, Charset charset) {
        return source.getCharSequence(index, length, charset);
    }
    public FriendlyByteBuf setBoolean(int index, boolean value) {
        source.setBoolean(index, value);
        return this;
    }
    public FriendlyByteBuf setByte(int index, int value) {
        source.setByte(index, value);
        return this;
    }
    public FriendlyByteBuf setShort(int index, int value) {
        source.setShort(index, value);
        return this;
    }
    public FriendlyByteBuf setShortLE(int index, int value) {
        source.setShortLE(index, value);
        return this;
    }
    public FriendlyByteBuf setMedium(int index, int value) {
        source.setMedium(index, value);
        return this;
    }
    public FriendlyByteBuf setMediumLE(int index, int value) {
        source.setMediumLE(index, value);
        return this;
    }
    public FriendlyByteBuf setInt(int index, int value) {
        source.setInt(index, value);
        return this;
    }
    public FriendlyByteBuf setIntLE(int index, int value) {
        source.setIntLE(index, value);
        return this;
    }
    public FriendlyByteBuf setLong(int index, long value) {
        source.setLong(index, value);
        return this;
    }
    public FriendlyByteBuf setLongLE(int index, long value) {
        source.setLongLE(index, value);
        return this;
    }
    public FriendlyByteBuf setChar(int index, int value) {
        source.setChar(index, value);
        return this;
    }
    public FriendlyByteBuf setFloat(int index, float value) {
        source.setFloat(index, value);
        return this;
    }
    public FriendlyByteBuf setDouble(int index, double value) {
        source.setDouble(index, value);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, ByteBuf src) {
        source.setBytes(index, src);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, ByteBuf src, int length) {
        source.setBytes(index, src, length);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, ByteBuf src, int srcIndex, int length) {
        source.setBytes(index, src, srcIndex, length);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, byte[] src) {
        source.setBytes(index, src);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, byte[] src, int srcIndex, int length) {
        source.setBytes(index, src, srcIndex, length);
        return this;
    }
    public FriendlyByteBuf setBytes(int index, ByteBuffer src) {
        source.setBytes(index, src);
        return this;
    }
    public int setBytes(int index, InputStream in, int length) throws IOException {
        return source.setBytes(index, in, length);
    }
    public int setBytes(int index, ScatteringByteChannel in, int length) throws IOException {
        return source.setBytes(index, in, length);
    }
    public int setBytes(int index, FileChannel in, long position, int length) throws IOException {
        return source.setBytes(index, in, position, length);
    }
    public FriendlyByteBuf setZero(int index, int length) {
        source.setZero(index, length);
        return this;
    }
    public int setCharSequence(int index, CharSequence sequence, Charset charset) {
        return source.setCharSequence(index, sequence, charset);
    }
    public boolean readBoolean() {
        return source.readBoolean();
    }
    public byte readByte() {
        return source.readByte();
    }
    public short readUnsignedByte() {
        return source.readUnsignedByte();
    }
    public short readShort() {
        return source.readShort();
    }
    public short readShortLE() {
        return source.readShortLE();
    }
    public int readUnsignedShort() {
        return source.readUnsignedShort();
    }
    public int readUnsignedShortLE() {
        return source.readUnsignedShortLE();
    }
    public int readMedium() {
        return source.readMedium();
    }
    public int readMediumLE() {
        return source.readMediumLE();
    }
    public int readUnsignedMedium() {
        return source.readUnsignedMedium();
    }
    public int readUnsignedMediumLE() {
        return source.readUnsignedMediumLE();
    }
    public int readInt() {
        return source.readInt();
    }
    public int readIntLE() {
        return source.readIntLE();
    }
    public long readUnsignedInt() {
        return source.readUnsignedInt();
    }
    public long readUnsignedIntLE() {
        return source.readUnsignedIntLE();
    }
    public long readLong() {
        return source.readLong();
    }
    public long readLongLE() {
        return source.readLongLE();
    }
    public char readChar() {
        return source.readChar();
    }
    public float readFloat() {
        return source.readFloat();
    }
    public double readDouble() {
        return source.readDouble();
    }
    public ByteBuf readBytes(int length) {
        source.readBytes(length);
        return this;
    }
    public ByteBuf readSlice(int length) {
        return new FriendlyByteBuf(source.readSlice(length));
    }
    public ByteBuf readRetainedSlice(int length) {
        return new FriendlyByteBuf(source.readRetainedSlice(length));
    }
    public FriendlyByteBuf readBytes(ByteBuf dst) {
        source.readBytes(dst);
        return this;
    }
    public FriendlyByteBuf readBytes(ByteBuf dst, int length) {
        source.readBytes(dst, length);
        return this;
    }
    public FriendlyByteBuf readBytes(ByteBuf dst, int dstIndex, int length) {
        source.readBytes(dst, dstIndex, length);
        return this;
    }
    public FriendlyByteBuf readBytes(byte[] dst) {
        source.readBytes(dst);
        return this;
    }
    public FriendlyByteBuf readBytes(byte[] dst, int dstIndex, int length) {
        source.readBytes(dst, dstIndex, length);
        return this;
    }
    public FriendlyByteBuf readBytes(ByteBuffer dst) {
        source.readBytes(dst);
        return this;
    }
    public FriendlyByteBuf readBytes(OutputStream out, int length) throws IOException {
        source.readBytes(out, length);
        return this;
    }
    public int readBytes(GatheringByteChannel out, int length) throws IOException {
        return source.readBytes(out, length);
    }
    public CharSequence readCharSequence(int length, Charset charset) {
        return source.readCharSequence(length, charset);
    }
    public String readString(int length, Charset charset) {
        String text = source.toString(source.readerIndex(), length, charset);
        source.readerIndex(source.readerIndex() + length);
        return text;
    }
    public int readBytes(FileChannel out, long position, int length) throws IOException {
        return source.readBytes(out, position, length);
    }
    public FriendlyByteBuf skipBytes(int length) {
        source.skipBytes(length);
        return this;
    }
    public FriendlyByteBuf writeBoolean(boolean value) {
        source.writeBoolean(value);
        return this;
    }
    public FriendlyByteBuf writeByte(int value) {
        source.writeByte(value);
        return this;
    }
    public FriendlyByteBuf writeShort(int value) {
        source.writeShort(value);
        return this;
    }
    public FriendlyByteBuf writeShortLE(int value) {
        source.writeShortLE(value);
        return this;
    }
    public FriendlyByteBuf writeMedium(int value) {
        source.writeMedium(value);
        return this;
    }
    public FriendlyByteBuf writeMediumLE(int value) {
        source.writeMediumLE(value);
        return this;
    }
    public FriendlyByteBuf writeInt(int value) {
        source.writeInt(value);
        return this;
    }
    public FriendlyByteBuf writeIntLE(int value) {
        source.writeIntLE(value);
        return this;
    }
    public FriendlyByteBuf writeLong(long value) {
        source.writeLong(value);
        return this;
    }
    public FriendlyByteBuf writeLongLE(long value) {
        source.writeLongLE(value);
        return this;
    }
    public FriendlyByteBuf writeChar(int value) {
        source.writeChar(value);
        return this;
    }
    public FriendlyByteBuf writeFloat(float value) {
        source.writeFloat(value);
        return this;
    }
    public FriendlyByteBuf writeDouble(double value) {
        source.writeDouble(value);
        return this;
    }
    public FriendlyByteBuf writeBytes(ByteBuf src) {
        source.writeBytes(src);
        return this;
    }
    public FriendlyByteBuf writeBytes(ByteBuf src, int length) {
        source.writeBytes(src, length);
        return this;
    }
    public FriendlyByteBuf writeBytes(ByteBuf src, int srcIndex, int length) {
        source.writeBytes(src, srcIndex, length);
        return this;
    }
    public FriendlyByteBuf writeBytes(byte[] src) {
        source.writeBytes(src);
        return this;
    }
    public FriendlyByteBuf writeBytes(byte[] src, int srcIndex, int length) {
        source.writeBytes(src, srcIndex, length);
        return this;
    }
    public FriendlyByteBuf writeBytes(ByteBuffer src) {
        source.writeBytes(src);
        return this;
    }
    public int writeBytes(InputStream in, int length) throws IOException {
        return source.writeBytes(in, length);
    }
    public int writeBytes(ScatteringByteChannel in, int length) throws IOException {
        return source.writeBytes(in, length);
    }
    public int writeBytes(FileChannel in, long position, int length) throws IOException {
        return source.writeBytes(in, position, length);
    }
    public FriendlyByteBuf writeZero(int length) {
        source.writeZero(length);
        return this;
    }
    public int writeCharSequence(CharSequence sequence, Charset charset) {
        return source.writeCharSequence(sequence, charset);
    }
    public int indexOf(int fromIndex, int toIndex, byte value) {
        return source.indexOf(fromIndex, toIndex, value);
    }
    public int bytesBefore(byte value) {
        return source.bytesBefore(value);
    }
    public int bytesBefore(int length, byte value) {
        return source.bytesBefore(length, value);
    }
    public int bytesBefore(int index, int length, byte value) {
        return source.bytesBefore(index, length, value);
    }
    public int forEachByte(ByteProcessor processor) {
        return source.forEachByte(processor);
    }
    public int forEachByte(int index, int length, ByteProcessor processor) {
        return source.forEachByte(index, length, processor);
    }
    public int forEachByteDesc(ByteProcessor processor) {
        return source.forEachByteDesc(processor);
    }
    public int forEachByteDesc(int index, int length, ByteProcessor processor) {
        return source.forEachByteDesc(index, length, processor);
    }
    public ByteBuf copy() {
        return new FriendlyByteBuf(source.copy());
    }
    public ByteBuf copy(int index, int length) {
        return new FriendlyByteBuf(source.copy(index, length));
    }
    public ByteBuf slice() {
        return new FriendlyByteBuf(source.slice());
    }
    public ByteBuf retainedSlice() {
        return new FriendlyByteBuf(source.retainedSlice());
    }
    public ByteBuf slice(int index, int length) {
        return new FriendlyByteBuf(source.slice(index, length));
    }
    public ByteBuf retainedSlice(int index, int length) {
        return new FriendlyByteBuf(source.retainedSlice(index, length));
    }
    public ByteBuf duplicate() {
        return new FriendlyByteBuf(source.duplicate());
    }
    public ByteBuf retainedDuplicate() {
        return new FriendlyByteBuf(source.retainedDuplicate());
    }
    public int nioBufferCount() {
        return source.nioBufferCount();
    }
    public ByteBuffer nioBuffer() {
        return source.nioBuffer();
    }
    public ByteBuffer nioBuffer(int index, int length) {
        return source.nioBuffer(index, length);
    }
    public ByteBuffer internalNioBuffer(int index, int length) {
        return source.internalNioBuffer(index, length);
    }
    public ByteBuffer[] nioBuffers() {
        return source.nioBuffers();
    }
    public ByteBuffer[] nioBuffers(int index, int length) {
        return source.nioBuffers(index, length);
    }
    public boolean hasArray() {
        return source.hasArray();
    }
    public byte[] array() {
        return source.array();
    }
    public int arrayOffset() {
        return source.arrayOffset();
    }
    public boolean hasMemoryAddress() {
        return source.hasMemoryAddress();
    }
    public long memoryAddress() {
        return source.memoryAddress();
    }
    public String toString(Charset charset) {
        return source.toString(charset);
    }
    public String toString(int index, int length, Charset charset) {
        return source.toString(index, length, charset);
    }
    public int hashCode() {
        return source.hashCode();
    }
    public boolean equals(Object obj) {
        return source.equals(obj);
    }
    public int compareTo(ByteBuf buffer) {
        return source.compareTo(buffer);
    }
    public String toString() {
        return source.toString();
    }
    public FriendlyByteBuf retain(int increment) {
        source.retain(increment);
        return this;
    }
    public FriendlyByteBuf retain() {
        source.retain();
        return this;
    }
    public FriendlyByteBuf touch() {
        source.touch();
        return this;
    }
    public FriendlyByteBuf touch(Object hint) {
        source.touch(hint);
        return this;
    }
    public int refCnt() {
        return source.refCnt();
    }
    public boolean release() {
        return source.release();
    }
    public boolean release(int decrement) {
        return source.release(decrement);
    }
}
