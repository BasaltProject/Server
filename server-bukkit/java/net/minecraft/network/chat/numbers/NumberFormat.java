// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import net.minecraft.network.chat.MutableComponent;

public interface NumberFormat {
    void write(io.netty.buffer.ByteBuf out);
    MutableComponent format(int value);
    NumberFormatType<? extends NumberFormat> type();
}
