package org.basaltmc.bukkit.net;

import io.netty.buffer.ByteBuf;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;

public final class PacketCodec {
    private PacketCodec() {}
    public static void varInt(ByteBuf out, int value) {
        do {
            int next = value & 127;
            value >>>= 7;
            out.writeByte(value == 0 ? next : next | 128);
        } while (value != 0);
    }
    public static int readVarInt(ByteBuf in) {
        int value = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            int next = in.readUnsignedByte();
            value |= (next & 127) << shift;
            if ((next & 128) == 0)
                return value;
        }
        throw new IllegalArgumentException("VarInt too long");
    }
    public static void varLong(ByteBuf out, long value) {
        do {
            int next = (int) (value & 127);
            value >>>= 7;
            out.writeByte(value == 0 ? next : next | 128);
        } while (value != 0);
    }
    public static long readVarLong(ByteBuf in) {
        long value = 0;
        for (int shift = 0; shift < 70; shift += 7) {
            int next = in.readUnsignedByte();
            value |= (long) (next & 127) << shift;
            if ((next & 128) == 0)
                return value;
        }
        throw new IllegalArgumentException("VarLong too long");
    }
    public static String readString(ByteBuf in) {
        int length = readVarInt(in);
        if (length < 0 || length > in.readableBytes())
            throw new IllegalArgumentException("Invalid string length");
        byte[] bytes = new byte[length];
        in.readBytes(bytes);
        return new String(bytes, StandardCharsets.UTF_8);
    }
    public static ByteBuf frame(WirePacket packet) {
        ByteBuf out = io.netty.buffer.Unpooled.buffer();
        try {
            varInt(out, packet.packetId());
            packet.write(out);
            return out;
        } catch (RuntimeException | Error e) {
            out.release();
            throw e;
        }
    }
    public static Object fromNative(int id, byte[] body) {
        if (id == 0x65) {
            ByteBuf in = io.netty.buffer.Unpooled.wrappedBuffer(body);
            try {
                return net.minecraft.network.protocol.game.ClientboundSetEntityDataPacket.read(in);
            } catch (RuntimeException ignored) { /* Unsupported/malformed metadata remains byte-for-byte intact. */
            } finally {
                in.release();
            }
        }
        ByteBuf out = io.netty.buffer.Unpooled.buffer(body.length + 5);
        varInt(out, id);
        out.writeBytes(body);
        return out;
    }
    public static void string(ByteBuf out, String value) {
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        varInt(out, bytes.length);
        out.writeBytes(bytes);
    }
    @SuppressWarnings("unchecked")
    public static void component(ByteBuf out, Component value) {
        Object tag = Texts.toNbt(value.asAdventure());
        // A compound root also represents Adventure's compact string/list forms.
        Map<String, Object> root = tag instanceof Map < ?, ? > ? (Map<String, Object>) tag
            : tag instanceof String                            ? Map.of("text", tag)
                                                               : Map.of("text", "", "extra", tag);
        out.writeBytes(Nbt.write(root));
    }
    public static void optionalFormat(ByteBuf out, NumberFormat format) {
        out.writeBoolean(format != null);
        if (format != null)
            format.write(out);
    }
}
