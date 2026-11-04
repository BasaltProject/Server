// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.nbt.Tag;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ServerboundCustomClickActionPacket(Identifier id, Optional<Tag> payload) implements Packet<ServerCommonPacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundCustomClickActionPacket> STREAM_CODEC = StreamCodec.composite(Identifier.STREAM_CODEC, ServerboundCustomClickActionPacket::id,
            ByteBufCodecs.optionalTagCodec(net.minecraft.nbt.NbtAccounter::defaultQuota), ServerboundCustomClickActionPacket::payload, ServerboundCustomClickActionPacket::new);
    public PacketType<ServerboundCustomClickActionPacket> type() {
        return CommonPacketTypes.SERVERBOUND_CUSTOM_CLICK_ACTION;
    }
    public void handle(ServerCommonPacketListener listener) {
        listener.handleCustomClickAction(this);
    }
}
