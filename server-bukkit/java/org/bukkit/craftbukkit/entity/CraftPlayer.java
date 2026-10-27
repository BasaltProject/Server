package org.bukkit.craftbukkit.entity;

import java.util.UUID;
import net.minecraft.network.Connection;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.network.ServerGamePacketListenerImpl;
import org.basaltmc.bukkit.BasaltPlayer;
import org.basaltmc.bukkit.net.ChannelBridge;

public class CraftPlayer extends BasaltPlayer {
    private ServerPlayer handle;

    public CraftPlayer(UUID id, String name) {
        super(id, name);
    }

    public synchronized ServerPlayer getHandle() {
        if (handle == null) {
            Connection connection = new Connection(ChannelBridge.channelOf(getUniqueId()));
            handle = new ServerPlayer(new ServerGamePacketListenerImpl(connection));
            handle.bindPlayer(this);
        }
        return handle;
    }
}
