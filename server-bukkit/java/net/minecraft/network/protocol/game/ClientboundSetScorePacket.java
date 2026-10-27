package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
public record ClientboundSetScorePacket(String owner, String objectiveName, int score, Optional<Component> display,
    Optional<NumberFormat> numberFormat) implements WirePacket {
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
}
