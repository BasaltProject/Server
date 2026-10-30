package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.world.scores.Objective;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
public class ClientboundSetObjectivePacket implements WirePacket {
    private final String objectiveName;
    private final int method, renderType;
    private final Component displayName;
    private final NumberFormat numberFormat;
    public ClientboundSetObjectivePacket(Objective objective, int method) {
        if (method < 0 || method > 2)
            throw new IllegalArgumentException("Invalid objective mode");
        objectiveName = objective.getName();
        this.method = method;
        displayName = objective.getDisplayName();
        renderType = objective.getRenderType().ordinal();
        numberFormat = objective.numberFormat();
    }
    public int packetId() {
        return 0x6c;
    }
    public void write(ByteBuf out) {
        PacketCodec.string(out, objectiveName);
        out.writeByte(method);
        if (method != 1) {
            PacketCodec.component(out, displayName);
            PacketCodec.varInt(out, renderType);
            PacketCodec.optionalFormat(out, numberFormat);
        }
    }
}
