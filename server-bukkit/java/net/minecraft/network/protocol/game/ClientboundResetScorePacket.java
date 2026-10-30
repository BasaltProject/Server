package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
public record ClientboundResetScorePacket(String owner, String objectiveName) implements WirePacket {
    public int packetId() {
        return 0x50;
    }
    public void write(ByteBuf out) {
        PacketCodec.string(out, owner);
        out.writeBoolean(objectiveName != null);
        if (objectiveName != null)
            PacketCodec.string(out, objectiveName);
    }
}
