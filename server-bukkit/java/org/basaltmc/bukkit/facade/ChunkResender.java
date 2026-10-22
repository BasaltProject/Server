package org.basaltmc.bukkit.facade;

import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientboundLevelChunkWithLightPacket;
import net.minecraft.server.level.ServerChunkCache;
import net.minecraft.server.network.ServerGamePacketListenerImpl;

public final class ChunkResender extends ServerGamePacketListenerImpl {
    private final String world;
    private final ServerChunkCache source;

    public ChunkResender(String world, ServerChunkCache source) {
        super(new Connection(null));
        this.world = world;
        this.source = source;
    }

    @Override
    public void send(Packet<?> packet) {
        if (packet instanceof ClientboundLevelChunkWithLightPacket chunk) {
            source.resend(chunk.x(), chunk.z());
        }
    }
}
