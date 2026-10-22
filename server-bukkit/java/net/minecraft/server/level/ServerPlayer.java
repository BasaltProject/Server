package net.minecraft.server.level;

import com.mojang.authlib.GameProfile;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerGamePacketListenerImpl;
import org.basaltmc.bukkit.Unsupported;

public class ServerPlayer {
    public ServerGamePacketListenerImpl connection;
    private ServerLevel level;

    public ServerPlayer(ServerGamePacketListenerImpl connection) {
        this.connection = connection;
    }

    public ServerPlayer(MinecraftServer server, ServerLevel level, GameProfile profile, ClientInformation information) {
        this.level = level;
    }

    public ServerLevel level() {
        if (level == null) {
            throw Unsupported.hit("net.minecraft.server.level.ServerPlayer.level");
        }
        return level;
    }
}
