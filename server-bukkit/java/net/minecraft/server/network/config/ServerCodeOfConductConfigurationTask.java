// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network.config;

import java.util.function.Consumer;
import java.util.function.Supplier;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.configuration.ClientboundCodeOfConductPacket;
import net.minecraft.server.network.ConfigurationTask;

public class ServerCodeOfConductConfigurationTask implements ConfigurationTask {
    public static final ConfigurationTask.Type TYPE = new ConfigurationTask.Type("server_code_of_conduct");
    private final Supplier<String> text;

    public ServerCodeOfConductConfigurationTask(Supplier<String> codeOfConduct) {
        this.text = codeOfConduct;
    }

    public void start(Consumer<Packet<?>> connection) {
        connection.accept(new ClientboundCodeOfConductPacket(text.get()));
    }

    public ConfigurationTask.Type type() {
        return TYPE;
    }
}
