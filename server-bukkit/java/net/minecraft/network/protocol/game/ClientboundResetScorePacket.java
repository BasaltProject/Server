// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;

public record ClientboundResetScorePacket(String owner, String objectiveName) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundResetScorePacket> STREAM_CODEC =
        Packet.codec(ClientboundResetScorePacket::write, ClientboundResetScorePacket::new);

    private ClientboundResetScorePacket(FriendlyByteBuf input) {
        this(input.readUtf(), input.readNullable(FriendlyByteBuf::readUtf));
    }

    private void write(FriendlyByteBuf output) {
        output.writeUtf(owner);
        output.writeNullable(objectiveName, FriendlyByteBuf::writeUtf);
    }

    public PacketType<ClientboundResetScorePacket> type() {
        return GamePacketTypes.CLIENTBOUND_RESET_SCORE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleResetScore(this);
    }
}
