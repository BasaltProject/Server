package net.minecraft.network.protocol.game;

import net.minecraft.network.protocol.common.ServerCommonPacketListener;
import net.minecraft.network.protocol.ping.ServerPingPacketListener;
public interface ServerGamePacketListener extends ServerCommonPacketListener, ServerPingPacketListener {
}
