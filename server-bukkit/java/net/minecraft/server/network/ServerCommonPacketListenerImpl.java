package net.minecraft.server.network;

import net.minecraft.network.Connection;

public class ServerCommonPacketListenerImpl {
    protected final Connection connection;

    public ServerCommonPacketListenerImpl(Connection connection) {
        this.connection = connection;
    }
}
