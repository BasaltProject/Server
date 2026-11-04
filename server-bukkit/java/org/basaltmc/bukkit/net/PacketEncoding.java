package org.basaltmc.bukkit.net;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import org.basaltmc.bukkit.facade.ServerAccess;

/** Turns facade packets into native wire packets using the packet class codec and the play id table. */
public final class PacketEncoding {
    private PacketEncoding() {}

    @SuppressWarnings("unchecked")
    private static StreamCodec<Object, Object> codecOf(Class<?> type) {
        for (Class<?> c = type; c != null; c = c.getSuperclass()) {
            for (String name : new String[] {"STREAM_CODEC", "GAMEPLAY_STREAM_CODEC"}) {
                try {
                    Field field = c.getDeclaredField(name);
                    if (Modifier.isStatic(field.getModifiers())) {
                        field.setAccessible(true);
                        return (StreamCodec<Object, Object>) field.get(null);
                    }
                } catch (NoSuchFieldException ignored) {
                    continue;
                } catch (IllegalAccessException e) {
                    throw new IllegalStateException(e);
                }
            }
        }
        return null;
    }

    public static WirePacket toWire(Packet<?> packet) {
        if (packet instanceof WirePacket wire) {
            return wire;
        }
        String name = packet.type().id().getPath();
        boolean serverbound = packet.type().flow() == net.minecraft.network.protocol.PacketFlow.SERVERBOUND;
        int id = serverbound ? PacketIds.serverbound(name) : PacketIds.clientbound(name);
        if (id < 0) {
            throw new IllegalArgumentException("Packet " + name + " is not part of the play protocol");
        }
        StreamCodec<Object, Object> codec = codecOf(packet.getClass());
        if (codec == null) {
            throw new UnsupportedOperationException(packet.getClass().getName() + " has no codec in the Basalt facade");
        }
        ByteBuf body = Unpooled.buffer();
        try {
            codec.encode(new RegistryFriendlyByteBuf(body, ServerAccess.registries()), packet);
            byte[] bytes = new byte[body.readableBytes()];
            body.readBytes(bytes);
            return new WirePacket() {
                @Override
                public int packetId() {
                    return id;
                }

                @Override
                public void write(ByteBuf out) {
                    out.writeBytes(bytes);
                }
            };
        } finally {
            body.release();
        }
    }
}
