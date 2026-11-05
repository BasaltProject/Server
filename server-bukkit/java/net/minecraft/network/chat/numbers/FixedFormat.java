// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;

public record FixedFormat(Component value) implements NumberFormat {
    public static final NumberFormatType<FixedFormat> TYPE = new NumberFormatType<>() {
        public MapCodec<FixedFormat> mapCodec() {
            return ComponentSerialization.CODEC.fieldOf("value").xmap(FixedFormat::new, FixedFormat::value);
        }

        public StreamCodec<RegistryFriendlyByteBuf, FixedFormat> streamCodec() {
            return StreamCodec.composite(ComponentSerialization.TRUSTED_STREAM_CODEC, FixedFormat::value, FixedFormat::new);
        }
    };

    public FixedFormat {
        java.util.Objects.requireNonNull(value);
    }

    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, NumberFormatTypes.FIXED_ID);
        org.basaltmc.bukkit.net.PacketCodec.component(out, value);
    }

    public MutableComponent format(int value) {
        return this.value.copy();
    }

    public NumberFormatType<FixedFormat> type() {
        return TYPE;
    }
}
