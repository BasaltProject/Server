// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import net.minecraft.network.chat.MutableComponent;
public final class BlankFormat implements NumberFormat {
    public static final BlankFormat INSTANCE = new BlankFormat();
    private BlankFormat() {}
    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, 0);
    }
    public static final NumberFormatType<BlankFormat> TYPE = null;
    public MutableComponent format(int value) {
        return net.minecraft.network.chat.Component.empty();
    }
    public NumberFormatType<BlankFormat> type() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.chat.numbers.BlankFormat#type");
    }
}
