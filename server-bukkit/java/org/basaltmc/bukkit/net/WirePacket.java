package org.basaltmc.bukkit.net;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.protocol.Packet;

/** Native-protocol packet bodies, sent through the same channel as ordinary packets. */
public interface WirePacket extends Packet<net.minecraft.network.protocol.game.ClientGamePacketListener> {
    int packetId();
    void write(ByteBuf out);

    default net.minecraft.network.protocol.PacketType<? extends Packet<net.minecraft.network.protocol.game.ClientGamePacketListener>> type() {
        throw new UnsupportedOperationException("A native wire packet has no vanilla packet type");
    }

    default void handle(net.minecraft.network.protocol.game.ClientGamePacketListener listener) {
        throw new UnsupportedOperationException("A native wire packet is only written to a connection");
    }
}
