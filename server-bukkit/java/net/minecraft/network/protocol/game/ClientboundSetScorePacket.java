// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.numbers.NumberFormatTypes;
import net.minecraft.network.codec.ByteBufCodecs;

import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundSetScorePacket(String owner, String objectiveName, int score, Optional<Component> display,
    Optional<NumberFormat> numberFormat) implements WirePacket, Packet<ClientGamePacketListener> {
    public ClientboundSetScorePacket {
        java.util.Objects.requireNonNull(owner);
        java.util.Objects.requireNonNull(objectiveName);
        java.util.Objects.requireNonNull(display);
        java.util.Objects.requireNonNull(numberFormat);
    }
    public int packetId() {
        return 0x70;
    }
    public void write(ByteBuf out) {
        PacketCodec.string(out, owner);
        PacketCodec.string(out, objectiveName);
        PacketCodec.varInt(out, score);
        out.writeBoolean(display.isPresent());
        display.ifPresent(value -> PacketCodec.component(out, value));
        PacketCodec.optionalFormat(out, numberFormat.orElse(null));
    }
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetScorePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetScorePacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.STRING_UTF8, ClientboundSetScorePacket::owner, ByteBufCodecs.STRING_UTF8, ClientboundSetScorePacket::objectiveName, ByteBufCodecs.VAR_INT, ClientboundSetScorePacket::score, ComponentSerialization.TRUSTED_OPTIONAL_STREAM_CODEC, ClientboundSetScorePacket::display, NumberFormatTypes.OPTIONAL_STREAM_CODEC, ClientboundSetScorePacket::numberFormat, ClientboundSetScorePacket::new));
    public PacketType<ClientboundSetScorePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_SCORE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetScore(this);
    }
}
