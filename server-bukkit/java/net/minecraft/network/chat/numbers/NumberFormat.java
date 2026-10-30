package net.minecraft.network.chat.numbers;
public interface NumberFormat {
    void write(io.netty.buffer.ByteBuf out);
}
