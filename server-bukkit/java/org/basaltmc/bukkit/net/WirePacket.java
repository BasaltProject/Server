package org.basaltmc.bukkit.net;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.protocol.Packet;

/** Native-protocol packet bodies, sent through the same channel as ordinary packets. */
public interface WirePacket extends Packet<net.minecraft.network.protocol.game.ClientGamePacketListener> {
    int packetId();
    void write(ByteBuf out);
}
