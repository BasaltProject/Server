// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
public record FixedFormat(net.minecraft.network.chat.Component value) implements NumberFormat {
    public FixedFormat {
        java.util.Objects.requireNonNull(value);
    }
    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, 2);
        org.basaltmc.bukkit.net.PacketCodec.component(out, value);
    }
    public static final NumberFormatType<FixedFormat> TYPE = null;
    public MutableComponent format(int value) {
        return this.value.copy();
    }
    public NumberFormatType<FixedFormat> type() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.chat.numbers.FixedFormat#type");
    }
}
