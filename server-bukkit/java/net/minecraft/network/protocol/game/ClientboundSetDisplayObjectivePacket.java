package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import net.minecraft.world.scores.DisplaySlot;
import net.minecraft.world.scores.Objective;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
public record ClientboundSetDisplayObjectivePacket(DisplaySlot slot, Objective objective) implements WirePacket {
    public int packetId() {
        return 0x64;
    }
    public void write(ByteBuf out) {
        PacketCodec.varInt(out, slot.id());
        PacketCodec.string(out, objective == null ? "" : objective.getName());
    }
}
