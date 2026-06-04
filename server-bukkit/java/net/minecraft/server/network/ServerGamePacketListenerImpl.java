package net.minecraft.server.network;

import net.minecraft.network.Connection;

public class ServerGamePacketListenerImpl extends ServerCommonPacketListenerImpl {
    public ServerGamePacketListenerImpl(Connection connection) {
        super(connection);
    }
}
