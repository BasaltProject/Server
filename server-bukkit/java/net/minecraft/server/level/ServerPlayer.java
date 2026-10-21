package net.minecraft.server.level;

import net.minecraft.server.network.ServerGamePacketListenerImpl;

public class ServerPlayer {
    public ServerGamePacketListenerImpl connection;

    public ServerPlayer(ServerGamePacketListenerImpl connection) {
        this.connection = connection;
    }
}
