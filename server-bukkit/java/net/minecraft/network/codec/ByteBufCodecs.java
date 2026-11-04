// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.codec;
import com.google.gson.JsonElement;
import com.mojang.authlib.GameProfile;
import com.mojang.authlib.properties.PropertyMap;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import io.netty.buffer.ByteBuf;
import java.security.PublicKey;
import java.time.Instant;
import java.util.BitSet;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.function.BiFunction;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import java.util.function.ToIntFunction;
import net.minecraft.core.Holder;
import net.minecraft.util.Mth;
import net.minecraft.core.HolderSet;
import net.minecraft.core.IdMap;
import net.minecraft.core.Registry;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.Tag;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.resources.ResourceKey;
import org.joml.Quaternionfc;
import org.joml.Vector3fc;

public interface ByteBufCodecs {
    int MAX_INITIAL_COLLECTION_SIZE = 65536;
    StreamCodec<ByteBuf, Boolean> BOOL = StreamCodec.of((buf, v) -> buf.writeBoolean(v), ByteBuf::readBoolean);
    StreamCodec<ByteBuf, Byte> BYTE = StreamCodec.of((buf, v) -> buf.writeByte(v), ByteBuf::readByte);
    StreamCodec<ByteBuf, Float> ROTATION_BYTE = BYTE.map(Mth::unpackDegrees, Mth::packDegrees);
    StreamCodec<ByteBuf, Short> SHORT = StreamCodec.of((buf, v) -> buf.writeShort(v), ByteBuf::readShort);
    StreamCodec<ByteBuf, Integer> UNSIGNED_SHORT = StreamCodec.of((buf, v) -> buf.writeShort(v), buf -> buf.readUnsignedShort());
    StreamCodec<ByteBuf, Integer> INT = StreamCodec.of((buf, v) -> buf.writeInt(v), ByteBuf::readInt);
    StreamCodec<ByteBuf, Integer> VAR_INT = StreamCodec.of((buf, v) -> writeVarInt(buf, v), ByteBufCodecs::readVarInt);
    StreamCodec<ByteBuf, OptionalInt> OPTIONAL_VAR_INT = VAR_INT.map(i -> i == 0 ? OptionalInt.empty() : OptionalInt.of(i - 1), o -> o.isPresent() ? o.getAsInt() + 1 : 0);
    StreamCodec<ByteBuf, Long> LONG = StreamCodec.of((buf, v) -> buf.writeLong(v), ByteBuf::readLong);
    StreamCodec<ByteBuf, Long> VAR_LONG = StreamCodec.of((buf, v) -> writeVarLong(buf, v), ByteBufCodecs::readVarLong);
    StreamCodec<ByteBuf, Float> FLOAT = StreamCodec.of((buf, v) -> buf.writeFloat(v), ByteBuf::readFloat);
    StreamCodec<ByteBuf, Double> DOUBLE = StreamCodec.of((buf, v) -> buf.writeDouble(v), ByteBuf::readDouble);
    StreamCodec<ByteBuf, byte[]> BYTE_ARRAY = byteArray(Integer.MAX_VALUE);
    StreamCodec<ByteBuf, long[]> LONG_ARRAY = StreamCodec.of(ByteBufCodecs::writeLongArray, ByteBufCodecs::readLongArray);
    StreamCodec<ByteBuf, BitSet> BIT_SET = LONG_ARRAY.map(BitSet::valueOf, BitSet::toLongArray);
    StreamCodec<ByteBuf, String> STRING_UTF8 = stringUtf8(32767);
    StreamCodec<ByteBuf, Tag> TAG = tagCodec(NbtAccounter::defaultQuota);
    StreamCodec<ByteBuf, Tag> TRUSTED_TAG = tagCodec(NbtAccounter::unlimitedHeap);
    StreamCodec<ByteBuf, CompoundTag> COMPOUND_TAG = compoundTagCodec(NbtAccounter::defaultQuota);
    StreamCodec<ByteBuf, CompoundTag> TRUSTED_COMPOUND_TAG = compoundTagCodec(NbtAccounter::unlimitedHeap);
    StreamCodec<ByteBuf, Optional<CompoundTag>> OPTIONAL_COMPOUND_TAG = optionalTagCodec(NbtAccounter::defaultQuota)
        .map(optional -> optional.map(tag -> (CompoundTag) tag), optional -> optional.map(tag -> (Tag) tag));
    StreamCodec<ByteBuf, Vector3fc> VECTOR3F = StreamCodec.of((buf, v) -> {
        buf.writeFloat(v.x());
        buf.writeFloat(v.y());
        buf.writeFloat(v.z());
    }, buf -> new org.joml.Vector3f(buf.readFloat(), buf.readFloat(), buf.readFloat()));
    StreamCodec<ByteBuf, Quaternionfc> QUATERNIONF = StreamCodec.of((buf, v) -> {
        buf.writeFloat(v.x());
        buf.writeFloat(v.y());
        buf.writeFloat(v.z());
        buf.writeFloat(v.w());
    }, buf -> new org.joml.Quaternionf(buf.readFloat(), buf.readFloat(), buf.readFloat(), buf.readFloat()));
    StreamCodec<ByteBuf, Integer> CONTAINER_ID = VAR_INT;
    StreamCodec<ByteBuf, PropertyMap> GAME_PROFILE_PROPERTIES = org.basaltmc.bukkit.facade.NetCodecs.propertyMap();
    StreamCodec<ByteBuf, String> PLAYER_NAME = stringUtf8(16);
    StreamCodec<ByteBuf, GameProfile> GAME_PROFILE = org.basaltmc.bukkit.facade.NetCodecs.gameProfile(GAME_PROFILE_PROPERTIES);
    StreamCodec<ByteBuf, Integer> RGB_COLOR = StreamCodec.of(
        (out, value) -> {
            out.writeByte(value >> 16);
            out.writeByte(value >> 8);
            out.writeByte(value);
        },
        in -> 0xFF000000 | (in.readByte() & 255) << 16 | (in.readByte() & 255) << 8 | (in.readByte() & 255));
    StreamCodec<ByteBuf, Instant> INSTANT = LONG.map(Instant::ofEpochMilli, Instant::toEpochMilli);
    int PUBLIC_KEY_SIZE = 256;
    int MAX_PUBLIC_KEY_HEADER_SIZE = 256;
    int MAX_PUBLIC_KEY_LENGTH = 512;
    StreamCodec<ByteBuf, PublicKey> PUBLIC_KEY = org.basaltmc.bukkit.facade.NetCodecs.publicKey(MAX_PUBLIC_KEY_LENGTH);
    static StreamCodec<ByteBuf, byte[]> byteArray(int maxSize) {
        return StreamCodec.of((buf, v) -> {
            if (v.length > maxSize) {
                throw new io.netty.handler.codec.EncoderException("ByteArray with size " + v.length + " is bigger than allowed " + maxSize);
            }
            writeVarInt(buf, v.length);
            buf.writeBytes(v);
        }, buf -> {
            int length = readVarInt(buf);
            if (length > maxSize) {
                throw new io.netty.handler.codec.DecoderException("ByteArray with size " + length + " is bigger than allowed " + maxSize);
            }
            byte[] out = new byte[length];
            buf.readBytes(out);
            return out;
        });
    }
    static StreamCodec<ByteBuf, String> stringUtf8(int maxStringLength) {
        return StreamCodec.of((buf, v) -> {
            byte[] bytes = v.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            if (bytes.length > maxStringLength * 3) {
                throw new io.netty.handler.codec.EncoderException("String too big (was " + bytes.length + " bytes encoded, max " + maxStringLength * 3 + ")");
            }
            writeVarInt(buf, bytes.length);
            buf.writeBytes(bytes);
        }, buf -> {
            int length = readVarInt(buf);
            if (length > maxStringLength * 3) {
                throw new io.netty.handler.codec.DecoderException("The received encoded string buffer length is longer than maximum allowed (" + length + " > " + maxStringLength * 3 + ")");
            }
            if (length < 0) {
                throw new io.netty.handler.codec.DecoderException("The received encoded string buffer length is less than zero! Weird string!");
            }
            String text = buf.toString(buf.readerIndex(), length, java.nio.charset.StandardCharsets.UTF_8);
            buf.skipBytes(length);
            if (text.length() > maxStringLength) {
                throw new io.netty.handler.codec.DecoderException("The received string length is longer than maximum allowed (" + text.length() + " > " + maxStringLength + ")");
            }
            return text;
        });
    }
    static StreamCodec<ByteBuf, BitSet> fixedBitSet(int size) {
        int bytes = (size + 7) / 8;
        return StreamCodec.of((buf, v) -> {
            if (v.length() > size) {
                throw new io.netty.handler.codec.EncoderException("BitSet is larger than expected size (" + v.length() + ">" + size + ")");
            }
            byte[] raw = java.util.Arrays.copyOf(v.toByteArray(), bytes);
            buf.writeBytes(raw);
        }, buf -> {
            byte[] raw = new byte[bytes];
            buf.readBytes(raw);
            return BitSet.valueOf(raw);
        });
    }
    static StreamCodec<ByteBuf, Optional<Tag>> optionalTagCodec(Supplier<NbtAccounter> accounter) {
        return StreamCodec.of((buf, value) -> net.minecraft.network.FriendlyByteBuf.writeNbt(buf, value.orElse(null)),
            buf -> Optional.ofNullable(net.minecraft.network.FriendlyByteBuf.readNbt(buf, accounter.get())));
    }
    static StreamCodec<ByteBuf, Tag> tagCodec(Supplier<NbtAccounter> accounter) {
        return optionalTagCodec(accounter).map(optional -> optional.orElseThrow(() -> new io.netty.handler.codec.DecoderException("Expected non-null compound tag")), Optional::of);
    }
    static StreamCodec<ByteBuf, CompoundTag> compoundTagCodec(Supplier<NbtAccounter> accounter) {
        return tagCodec(accounter).map(tag -> {
            if (tag instanceof CompoundTag compound) {
        return compound;
            }
            throw new io.netty.handler.codec.DecoderException("Not a compound tag: " + tag);
        }, compound -> compound);
    }
    static <T> StreamCodec<ByteBuf, T> fromCodecTrusted(Codec<T> codec) {
        return fromCodec(codec, NbtAccounter::unlimitedHeap);
    }
    static <T> StreamCodec<ByteBuf, T> fromCodec(Codec<T> codec) {
        return fromCodec(codec, NbtAccounter::defaultQuota);
    }
    static <T, B extends ByteBuf, V> StreamCodec.CodecOperation<B, T, V> fromCodec(DynamicOps<T> ops, Codec<V> codec) {
        return original -> original.map(
            value -> codec.parse(ops, value).getOrThrow(message -> new io.netty.handler.codec.DecoderException("Failed to decode: " + message + " " + value)),
            value -> codec.encodeStart(ops, value).getOrThrow(message -> new io.netty.handler.codec.EncoderException("Failed to encode: " + message + " " + value)));
    }
    static <T> StreamCodec<ByteBuf, T> fromCodec(Codec<T> codec, Supplier<NbtAccounter> accounter) {
        return ByteBufCodecs.<Tag, ByteBuf, T>fromCodec(net.minecraft.nbt.NbtOps.INSTANCE, codec).apply(tagCodec(accounter));
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, T> fromCodecWithRegistriesTrusted(Codec<T> codec) {
        return fromCodecWithRegistries(codec, NbtAccounter::unlimitedHeap);
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, T> fromCodecWithRegistries(Codec<T> codec) {
        return fromCodecWithRegistries(codec, NbtAccounter::defaultQuota);
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, T> fromCodecWithRegistries(Codec<T> codec, Supplier<NbtAccounter> accounter) {
        StreamCodec<ByteBuf, Tag> tags = tagCodec(accounter);
        return StreamCodec.of((buf, value) -> tags.encode(buf, codec.encodeStart(
            net.minecraft.resources.RegistryOps.create(net.minecraft.nbt.NbtOps.INSTANCE, buf.registryAccess()), value)
            .getOrThrow(message -> new io.netty.handler.codec.EncoderException("Failed to encode: " + message + " " + value))),
            buf -> {
        Tag tag = tags.decode(buf);
        return codec.parse(net.minecraft.resources.RegistryOps.create(net.minecraft.nbt.NbtOps.INSTANCE, buf.registryAccess()), tag)
            .getOrThrow(message -> new io.netty.handler.codec.DecoderException("Failed to decode: " + message + " " + tag));
            });
    }
    static <B extends ByteBuf, V> StreamCodec<B, Optional<V>> optional(StreamCodec<? super B, V> original) {
        return StreamCodec.of((buf, v) -> {
            if (v.isPresent()) {
                buf.writeBoolean(true);
                original.encode(buf, v.get());
            } else {
                buf.writeBoolean(false);
            }
        }, buf -> buf.readBoolean() ? Optional.of(original.decode(buf)) : Optional.empty());
    }
    static int readCount(ByteBuf input, int maxSize) {
        int count = readVarInt(input);
        if (count > maxSize) {
            throw new io.netty.handler.codec.DecoderException(count + " elements exceeded max size of: " + maxSize);
        }
        return count;
    }
    static void writeCount(ByteBuf output, int count, int maxSize) {
        if (count > maxSize) {
            throw new io.netty.handler.codec.EncoderException(count + " elements exceeded max size of: " + maxSize);
        }
        writeVarInt(output, count);
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec<B, C> collection(IntFunction<C> constructor, StreamCodec<? super B, V> elementCodec) {
        return collection(constructor, elementCodec, Integer.MAX_VALUE);
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec<B, C> collection(IntFunction<C> constructor, StreamCodec<? super B, V> elementCodec, int maxSize) {
        return StreamCodec.of((buf, values) -> {
            writeCount(buf, values.size(), maxSize);
            for (V element : values) {
                elementCodec.encode(buf, element);
            }
        }, buf -> {
            int count = readCount(buf, maxSize);
            C out = constructor.apply(Math.min(count, MAX_INITIAL_COLLECTION_SIZE));
            for (int i = 0; i < count; i++) {
                out.add(elementCodec.decode(buf));
            }
            return out;
        });
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec.CodecOperation<B, V, C> collection(IntFunction<C> constructor) {
        return codec -> collection(constructor, codec);
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec.CodecOperation<B, V, C> collection(IntFunction<C> constructor, int maxSize) {
        return codec -> collection(constructor, codec, maxSize);
    }
    static <B extends ByteBuf, V> StreamCodec.CodecOperation<B, V, List<V>> list() {
        return codec -> collection(java.util.ArrayList::new, codec);
    }
    static <B extends ByteBuf, V> StreamCodec.CodecOperation<B, V, List<V>> list(int maxSize) {
        return codec -> collection(java.util.ArrayList::new, codec, maxSize);
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec<B, C> fixedSizeCollection(IntFunction<C> constructor, StreamCodec<? super B, V> elementCodec, int size) {
        return StreamCodec.of((buf, values) -> {
            if (values.size() != size) {
                throw new io.netty.handler.codec.EncoderException("Expected a collection of size " + size + " but got " + values.size());
            }
            for (V element : values) {
                elementCodec.encode(buf, element);
            }
        }, buf -> {
            C out = constructor.apply(size);
            for (int i = 0; i < size; i++) {
                out.add(elementCodec.decode(buf));
            }
            return out;
        });
    }
    static <B extends ByteBuf, V, C extends Collection<V>> StreamCodec.CodecOperation<B, V, C> fixedSizeCollection(IntFunction<C> constructor, int size) {
        return codec -> fixedSizeCollection(constructor, codec, size);
    }
    static <B extends ByteBuf, V> StreamCodec.CodecOperation<B, V, List<V>> fixedSizeList(int size) {
        return codec -> fixedSizeCollection(java.util.ArrayList::new, codec, size);
    }
    static <B extends ByteBuf, K, V, M extends Map<K, V>> StreamCodec<B, M> map(IntFunction<? extends M> constructor, StreamCodec<? super B, K> keyCodec, StreamCodec<? super B, V> valueCodec) {
        return map(constructor, keyCodec, valueCodec, Integer.MAX_VALUE);
    }
    static <B extends ByteBuf, K, V, M extends Map<K, V>> StreamCodec<B, M> map(IntFunction<? extends M> constructor, StreamCodec<? super B, K> keyCodec, StreamCodec<? super B, V> valueCodec, int maxSize) {
        return StreamCodec.of((buf, values) -> {
            writeCount(buf, values.size(), maxSize);
            values.forEach((key, value) -> {
                keyCodec.encode(buf, key);
                valueCodec.encode(buf, value);
            });
        }, buf -> {
            int count = readCount(buf, maxSize);
            M out = constructor.apply(Math.min(count, MAX_INITIAL_COLLECTION_SIZE));
            for (int j = 0; j < count; j++) {
                out.put(keyCodec.decode(buf), valueCodec.decode(buf));
            }
            return out;
        });
    }
    static <B extends ByteBuf, L, R> StreamCodec<B, Either<L, R>> either(StreamCodec<? super B, L> leftCodec, StreamCodec<? super B, R> rightCodec) {
        return StreamCodec.of((buf, v) -> v.ifLeft(left -> {
            buf.writeBoolean(true);
            leftCodec.encode(buf, left);
        }).ifRight(right -> {
            buf.writeBoolean(false);
            rightCodec.encode(buf, right);
        }), buf -> buf.readBoolean() ? Either.left(leftCodec.decode(buf)) : Either.right(rightCodec.decode(buf)));
    }
    static <B extends ByteBuf, V> StreamCodec.CodecOperation<B, V, V> lengthPrefixed(int maxSize, BiFunction<B, ByteBuf, B> decorator) {
        return original -> StreamCodec.of((buf, value) -> {
            ByteBuf scratch = buf.alloc().buffer();
            try {
        original.encode(decorator.apply(buf, scratch), value);
        int length = scratch.readableBytes();
        if (length > maxSize) {
            throw new io.netty.handler.codec.EncoderException("Buffer size " + length + " is larger than allowed limit of " + maxSize);
        }
        writeVarInt(buf, length);
        buf.writeBytes(scratch);
            } finally {
        scratch.release();
            }
        }, buf -> {
            int length = readVarInt(buf);
            if (length > maxSize) {
        throw new io.netty.handler.codec.DecoderException("Buffer size " + length + " is larger than allowed limit of " + maxSize);
            }
            ByteBuf slice = buf.readSlice(length);
            return original.decode(decorator.apply(buf, slice));
        });
    }
    static <V> StreamCodec.CodecOperation<ByteBuf, V, V> lengthPrefixed(int maxSize) {
        return lengthPrefixed(maxSize, (buf, scratch) -> scratch);
    }
    static <V> StreamCodec.CodecOperation<RegistryFriendlyByteBuf, V, V> registryFriendlyLengthPrefixed(int maxSize) {
        return lengthPrefixed(maxSize, (buf, scratch) -> new RegistryFriendlyByteBuf(scratch, buf.registryAccess()));
    }
    static <T> StreamCodec<ByteBuf, T> idMapper(IntFunction<T> byId, ToIntFunction<T> toId) {
        return StreamCodec.of((buf, v) -> writeVarInt(buf, toId.applyAsInt(v)), buf -> byId.apply(readVarInt(buf)));
    }
    static <T> StreamCodec<ByteBuf, T> idMapper(IdMap<T> mapper) {
        return idMapper(mapper::byIdOrThrow, mapper::getIdOrThrow);
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, T> registry(ResourceKey<? extends Registry<T>> registryKey) {
        return StreamCodec.of((buf, value) -> writeVarInt(buf, buf.registryAccess().<T>lookupOrThrow(registryKey).getIdOrThrow(value)),
            buf -> buf.registryAccess().<T>lookupOrThrow(registryKey).byIdOrThrow(readVarInt(buf)));
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, Holder<T>> holderRegistry(ResourceKey<? extends Registry<T>> registryKey) {
        return StreamCodec.of((buf, holder) -> writeVarInt(buf, buf.registryAccess().<T>lookupOrThrow(registryKey).asHolderIdMap().getIdOrThrow(holder)),
            buf -> buf.registryAccess().<T>lookupOrThrow(registryKey).asHolderIdMap().byIdOrThrow(readVarInt(buf)));
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, Holder<T>> holder(ResourceKey<? extends Registry<T>> registryKey, StreamCodec<? super RegistryFriendlyByteBuf, T> directCodec) {
        return StreamCodec.of((buf, holder) -> {
            if (holder.kind() == Holder.Kind.REFERENCE) {
        writeVarInt(buf, buf.registryAccess().<T>lookupOrThrow(registryKey).asHolderIdMap().getIdOrThrow(holder) + 1);
            } else {
        writeVarInt(buf, 0);
        directCodec.encode(buf, holder.value());
            }
        }, buf -> {
            int id = readVarInt(buf);
            if (id == 0) {
        return Holder.direct(directCodec.decode(buf));
            }
            return buf.registryAccess().<T>lookupOrThrow(registryKey).asHolderIdMap().byIdOrThrow(id - 1);
        });
    }
    static <T> StreamCodec<RegistryFriendlyByteBuf, HolderSet<T>> holderSet(ResourceKey<? extends Registry<T>> registryKey) {
        return StreamCodec.of((buf, set) -> {
            if (set.unwrap().left().isPresent()) {
        writeVarInt(buf, 0);
        buf.writeIdentifier(set.unwrap().left().get().location());
            } else {
        List<Holder<T>> holders = set.unwrap().right().get();
        writeVarInt(buf, holders.size() + 1);
        IdMap<Holder<T>> ids = buf.registryAccess().<T>lookupOrThrow(registryKey).asHolderIdMap();
        for (Holder<T> holder : holders) {
            writeVarInt(buf, ids.getIdOrThrow(holder));
        }
            }
        }, buf -> {
            int size = readVarInt(buf) - 1;
            Registry<T> registry = buf.registryAccess().<T>lookupOrThrow(registryKey);
            if (size == -1) {
        net.minecraft.tags.TagKey<T> key = net.minecraft.tags.TagKey.create(registryKey, buf.readIdentifier());
        return registry.get(key).<HolderSet<T>>map(named -> named).orElseGet(() -> HolderSet.emptyNamed(registry, key));
            }
            List<Holder<T>> holders = new java.util.ArrayList<>(size);
            for (int i = 0; i < size; i++) {
        holders.add(registry.asHolderIdMap().byIdOrThrow(readVarInt(buf)));
            }
            return HolderSet.direct(holders);
        });
    }
    static StreamCodec<ByteBuf, JsonElement> lenientJson(int maxStringLength) {
        return stringUtf8(maxStringLength).map(text -> com.google.gson.JsonParser.parseString(text),
            json -> new com.google.gson.Gson().toJson(json));
    }

    private static void writeVarInt(ByteBuf buf, int value) {
        int rest = value;
        while ((rest & ~0x7F) != 0) {
            buf.writeByte(rest & 0x7F | 0x80);
            rest >>>= 7;
        }
        buf.writeByte(rest);
    }

    private static int readVarInt(ByteBuf buf) {
        int value = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            byte next = buf.readByte();
            value |= (next & 0x7F) << shift;
            if ((next & 0x80) == 0) {
                return value;
            }
        }
        throw new RuntimeException("VarInt too big");
    }

    private static void writeVarLong(ByteBuf buf, long value) {
        long rest = value;
        while ((rest & ~0x7FL) != 0L) {
            buf.writeByte((int) (rest & 0x7F) | 0x80);
            rest >>>= 7;
        }
        buf.writeByte((int) rest);
    }

    private static long readVarLong(ByteBuf buf) {
        long value = 0L;
        for (int shift = 0; shift < 70; shift += 7) {
            byte next = buf.readByte();
            value |= (long) (next & 0x7F) << shift;
            if ((next & 0x80) == 0) {
                return value;
            }
        }
        throw new RuntimeException("VarLong too big");
    }

    private static void writeLongArray(ByteBuf buf, long[] values) {
        writeVarInt(buf, values.length);
        for (long v : values) {
            buf.writeLong(v);
        }
    }

    private static long[] readLongArray(ByteBuf buf) {
        long[] out = new long[readVarInt(buf)];
        for (int i = 0; i < out.length; i++) {
            out[i] = buf.readLong();
        }
        return out;
    }
}
