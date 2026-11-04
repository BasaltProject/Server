// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import net.minecraft.network.chat.MutableComponent;
public interface NumberFormat {
    default void write(io.netty.buffer.ByteBuf out) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.chat.numbers.NumberFormat#write");
    }
    default MutableComponent format(int value) {
        return net.minecraft.network.chat.Component.literal(Integer.toString(value));
    }
    default NumberFormatType<? extends NumberFormat> type() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.chat.numbers.NumberFormat#type");
    }
}
