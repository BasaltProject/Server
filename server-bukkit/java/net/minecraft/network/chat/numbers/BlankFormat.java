package net.minecraft.network.chat.numbers;
public final class BlankFormat implements NumberFormat {
    public static final BlankFormat INSTANCE = new BlankFormat();
    private BlankFormat() {}
    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, 0);
    }
}
