// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import net.minecraft.network.chat.Component;
public class RateKickingConnection extends Connection {
    private final int limit;
    public RateKickingConnection(int rateLimitPacketsPerSecond) {
        super((io.netty.channel.Channel) null);
        this.limit = rateLimitPacketsPerSecond;
    }
    protected void tickSecond() {
        super.tickSecond();
        if (getAverageReceivedPackets() > limit) {
            disconnect(Component.translatable("disconnect.exceeded_packet_rate"));
            setReadOnly();
        }
    }
}
