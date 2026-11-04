// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import io.netty.buffer.ByteBuf;
import java.util.Map;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundCustomReportDetailsPacket(Map<String, String> details) implements Packet<ClientCommonPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundCustomReportDetailsPacket> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.<ByteBuf, String, String, java.util.HashMap<String, String>>map(java.util.HashMap::new, net.minecraft.network.codec.ByteBufCodecs.stringUtf8(128), net.minecraft.network.codec.ByteBufCodecs.stringUtf8(4096), 32).map(map -> new ClientboundCustomReportDetailsPacket(map), packet -> new java.util.HashMap<>(packet.details()));
    public PacketType<ClientboundCustomReportDetailsPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_CUSTOM_REPORT_DETAILS;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleCustomReportDetails(this);
    }
}
