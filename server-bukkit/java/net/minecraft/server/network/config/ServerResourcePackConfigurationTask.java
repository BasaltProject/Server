// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network.config;

import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.common.ClientboundResourcePackPushPacket;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ConfigurationTask;

public class ServerResourcePackConfigurationTask implements ConfigurationTask {
    public static final ConfigurationTask.Type TYPE = new ConfigurationTask.Type("server_resource_pack");
    private final MinecraftServer.ServerResourcePackInfo pack;

    public ServerResourcePackConfigurationTask(MinecraftServer.ServerResourcePackInfo info) {
        this.pack = info;
    }

    public void start(Consumer<Packet<?>> connection) {
        connection.accept(new ClientboundResourcePackPushPacket(pack.id(), pack.url(), pack.hash(), pack.isRequired(), Optional.ofNullable(pack.prompt())));
    }

    public ConfigurationTask.Type type() {
        return TYPE;
    }
}
