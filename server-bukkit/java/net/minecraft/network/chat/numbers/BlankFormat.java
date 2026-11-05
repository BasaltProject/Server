// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;

public final class BlankFormat implements NumberFormat {
    public static final BlankFormat INSTANCE = new BlankFormat();
    public static final NumberFormatType<BlankFormat> TYPE = new NumberFormatType<>() {
        public MapCodec<BlankFormat> mapCodec() {
            return MapCodec.unit(INSTANCE);
        }

        public StreamCodec<RegistryFriendlyByteBuf, BlankFormat> streamCodec() {
            return StreamCodec.unit(INSTANCE);
        }
    };

    private BlankFormat() {}

    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, NumberFormatTypes.BLANK_ID);
    }

    public MutableComponent format(int value) {
        return Component.empty();
    }

    public NumberFormatType<BlankFormat> type() {
        return TYPE;
    }
}
