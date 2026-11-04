// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.google.common.collect.Sets;
import com.mojang.authlib.GameProfile;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DataResult;
import io.netty.buffer.ByteBuf;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Set;
import java.util.UUID;
import java.util.stream.IntStream;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.Util;

public class UUIDUtil {
    public static final int UUID_BYTES = 16;
    private static final String UUID_PREFIX_OFFLINE_PLAYER = "OfflinePlayer:";
    public static final Codec<UUID> CODEC = Codec.INT_STREAM.comapFlatMap(
        stream -> Util.fixedSize(stream, 4).map(UUIDUtil::uuidFromIntArray),
        uuid -> Arrays.stream(uuidToIntArray(uuid)));
    public static final Codec<Set<UUID>> CODEC_SET = Codec.list(CODEC).xmap(list -> (Set<UUID>) new java.util.HashSet<>(list), set -> new java.util.ArrayList<>(set));
    public static final Codec<Set<UUID>> CODEC_LINKED_SET = Codec.list(CODEC).xmap(list -> (Set<UUID>) new java.util.LinkedHashSet<>(list), set -> new java.util.ArrayList<>(set));
    public static final Codec<UUID> STRING_CODEC = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(UUID.fromString(text), com.mojang.serialization.Lifecycle.stable());
        } catch (IllegalArgumentException e) {
            return DataResult.error(() -> "Invalid UUID " + text + ": " + e.getMessage());
        }
    }, UUID::toString);
    public static final Codec<UUID> AUTHLIB_CODEC = Codec.withAlternative(Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(UUID.fromString(text));
        } catch (IllegalArgumentException e) {
            return DataResult.error(() -> "Invalid UUID " + text);
        }
    }, UUID::toString), CODEC);
    public static final Codec<UUID> LENIENT_CODEC = Codec.withAlternative(CODEC, STRING_CODEC);
    public static final StreamCodec<ByteBuf, UUID> STREAM_CODEC = StreamCodec.of(
        (buf, uuid) -> {
            buf.writeLong(uuid.getMostSignificantBits());
            buf.writeLong(uuid.getLeastSignificantBits());
        }, buf -> new UUID(buf.readLong(), buf.readLong()));

    protected UUIDUtil() {}

    public static UUID uuidFromIntArray(int[] intArray) {
        return new UUID((long) intArray[0] << 32 | intArray[1] & 4294967295L, (long) intArray[2] << 32 | intArray[3] & 4294967295L);
    }

    public static int[] uuidToIntArray(UUID uuid) {
        long most = uuid.getMostSignificantBits();
        long least = uuid.getLeastSignificantBits();
        return new int[] {(int) (most >> 32), (int) most, (int) (least >> 32), (int) least};
    }

    public static byte[] uuidToByteArray(UUID uuid) {
        byte[] bytes = new byte[16];
        ByteBuffer.wrap(bytes).putLong(uuid.getMostSignificantBits()).putLong(uuid.getLeastSignificantBits());
        return bytes;
    }

    public static UUID readUUID(Dynamic<?> input) {
        int[] ints = input.asIntStream().toArray();
        if (ints.length != 4) {
            throw new IllegalArgumentException("Could not read UUID. Expected int-array of length 4, got " + ints.length + ".");
        }
        return uuidFromIntArray(ints);
    }

    public static UUID createOfflinePlayerUUID(String playerName) {
        return UUID.nameUUIDFromBytes((UUID_PREFIX_OFFLINE_PLAYER + playerName).getBytes(StandardCharsets.UTF_8));
    }

    public static GameProfile createOfflineProfile(String playerName) {
        return new GameProfile(createOfflinePlayerUUID(playerName), playerName);
    }
}
