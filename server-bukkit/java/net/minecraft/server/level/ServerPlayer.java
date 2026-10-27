package net.minecraft.server.level;

import com.mojang.authlib.GameProfile;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerGamePacketListenerImpl;
import org.basaltmc.bukkit.Unsupported;

public class ServerPlayer extends net.minecraft.world.entity.player.Player {
    private org.bukkit.entity.Player player;
    public void bindPlayer(org.bukkit.entity.Player player){this.player=player;bindBukkitEntity(player);}
    @Override public int getId(){return player==null?super.getId():player.getEntityId();}
    @Override public java.util.UUID getUUID(){return player==null?super.getUUID():player.getUniqueId();}
    @Override public org.bukkit.craftbukkit.entity.CraftEntity getBukkitEntity(){return player==null?super.getBukkitEntity():(org.bukkit.craftbukkit.entity.CraftEntity)player;}

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
