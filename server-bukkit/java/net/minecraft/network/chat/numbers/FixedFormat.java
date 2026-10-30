package net.minecraft.network.chat.numbers;
public record FixedFormat(net.minecraft.network.chat.Component value) implements NumberFormat {
    public FixedFormat {
        java.util.Objects.requireNonNull(value);
    }
    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, 2);
        org.basaltmc.bukkit.net.PacketCodec.component(out, value);
    }
}
