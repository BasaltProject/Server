package net.minecraft.server;

import net.minecraft.server.network.ServerConnectionListener;

/** The slice of the vanilla server class that plugins reach by reflection to find the network layer. */
public class MinecraftServer {
    private final ServerConnectionListener connection = new ServerConnectionListener();

    public boolean isDebugging() {
        return false;
    }

    public ServerConnectionListener getConnection() {
        return connection;
    }
}
