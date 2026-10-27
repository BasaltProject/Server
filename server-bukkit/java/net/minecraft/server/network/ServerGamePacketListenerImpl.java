package net.minecraft.server.network;

import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import org.basaltmc.bukkit.Unsupported;

public class ServerGamePacketListenerImpl extends ServerCommonPacketListenerImpl {
    public ServerGamePacketListenerImpl(Connection connection) {
        super(connection);
    }

}
