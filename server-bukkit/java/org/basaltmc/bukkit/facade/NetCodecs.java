package org.basaltmc.bukkit.facade;

import com.mojang.authlib.GameProfile;
import com.mojang.authlib.properties.Property;
import com.mojang.authlib.properties.PropertyMap;
import io.netty.buffer.ByteBuf;
import io.netty.handler.codec.DecoderException;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.spec.X509EncodedKeySpec;
import java.util.UUID;
import net.minecraft.network.codec.StreamCodec;

public final class NetCodecs {
    private NetCodecs() {}

    private static int readVarInt(ByteBuf buf) {
        int result = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            byte current = buf.readByte();
            result |= (current & 127) << shift;
            if ((current & 128) == 0) {
                return result;
            }
        }
        throw new DecoderException("VarInt too big");
    }

    private static void writeVarInt(ByteBuf buf, int value) {
        while ((value & -128) != 0) {
            buf.writeByte(value & 127 | 128);
            value >>>= 7;
        }
        buf.writeByte(value);
    }

    private static String readString(ByteBuf buf, int maxLength) {
        int length = readVarInt(buf);
        if (length > maxLength * 4 || length < 0) {
            throw new DecoderException("String buffer length out of range: " + length);
        }
        byte[] bytes = new byte[length];
        buf.readBytes(bytes);
        return new String(bytes, java.nio.charset.StandardCharsets.UTF_8);
    }

    private static void writeString(ByteBuf buf, String value) {
        byte[] bytes = value.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        writeVarInt(buf, bytes.length);
        buf.writeBytes(bytes);
    }

    public static StreamCodec<ByteBuf, PropertyMap> propertyMap() {
        return StreamCodec.of((buf, properties) -> {
            writeVarInt(buf, properties.size());
            for (Property property : properties.values()) {
                writeString(buf, property.name());
                writeString(buf, property.value());
                buf.writeBoolean(property.signature() != null);
                if (property.signature() != null) {
                    writeString(buf, property.signature());
                }
            }
        }, buf -> {
            int count = readVarInt(buf);
            com.google.common.collect.Multimap<String, Property> map = com.google.common.collect.LinkedHashMultimap.create();
            for (int i = 0; i < count; i++) {
                String name = readString(buf, 64);
                String value = readString(buf, 32767);
                String signature = buf.readBoolean() ? readString(buf, 1024) : null;
                map.put(name, new Property(name, value, signature));
            }
            return new PropertyMap(map);
        });
    }

    public static StreamCodec<ByteBuf, GameProfile> gameProfile(StreamCodec<ByteBuf, PropertyMap> properties) {
        return StreamCodec.of((buf, profile) -> {
            buf.writeLong(profile.id().getMostSignificantBits());
            buf.writeLong(profile.id().getLeastSignificantBits());
            writeString(buf, profile.name());
            properties.encode(buf, profile.properties());
        }, buf -> new GameProfile(new UUID(buf.readLong(), buf.readLong()), readString(buf, 16), properties.decode(buf)));
    }

    public static StreamCodec<ByteBuf, PublicKey> publicKey(int maxLength) {
        return StreamCodec.of((buf, key) -> {
            byte[] encoded = key.getEncoded();
            writeVarInt(buf, encoded.length);
            buf.writeBytes(encoded);
        }, buf -> {
            int length = readVarInt(buf);
            if (length > maxLength) {
                throw new DecoderException("Public key too large: " + length);
            }
            byte[] encoded = new byte[length];
            buf.readBytes(encoded);
            try {
                return KeyFactory.getInstance("RSA").generatePublic(new X509EncodedKeySpec(encoded));
            } catch (java.security.GeneralSecurityException e) {
                throw new DecoderException("Malformed public key", e);
            }
        });
    }
}
