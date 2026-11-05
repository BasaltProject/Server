// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import java.util.function.Consumer;
import net.minecraft.network.protocol.Packet;

public interface ConfigurationTask {
    void start(Consumer<Packet<?>> connection);

    default boolean tick() {
        return true;
    }

    Type type();

    public static record Type(String id) {
        public String toString() {
            return id;
        }
    }
}
