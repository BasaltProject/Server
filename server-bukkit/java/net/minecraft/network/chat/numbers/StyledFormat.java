// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import com.mojang.serialization.MapCodec;
import net.minecraft.ChatFormatting;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.Style;
import net.minecraft.network.codec.StreamCodec;

public record StyledFormat(Style style) implements NumberFormat {
    public static final NumberFormatType<StyledFormat> TYPE = new NumberFormatType<>() {
        public MapCodec<StyledFormat> mapCodec() {
            return Style.Serializer.MAP_CODEC.xmap(StyledFormat::new, StyledFormat::style);
        }

        public StreamCodec<RegistryFriendlyByteBuf, StyledFormat> streamCodec() {
            return StreamCodec.composite(Style.Serializer.TRUSTED_STREAM_CODEC, StyledFormat::style, StyledFormat::new);
        }
    };
    public static final StyledFormat NO_STYLE = new StyledFormat(Style.EMPTY);
    public static final StyledFormat SIDEBAR_DEFAULT = new StyledFormat(Style.EMPTY.withColor(ChatFormatting.RED));
    public static final StyledFormat PLAYER_LIST_DEFAULT = new StyledFormat(Style.EMPTY.withColor(ChatFormatting.YELLOW));

    public void write(io.netty.buffer.ByteBuf out) {
        org.basaltmc.bukkit.net.PacketCodec.varInt(out, NumberFormatTypes.STYLED_ID);
        TYPE.streamCodec().encode(new net.minecraft.network.RegistryFriendlyByteBuf(out, null), this);
    }

    public MutableComponent format(int value) {
        return Component.literal(Integer.toString(value)).withStyle(style);
    }

    public NumberFormatType<StyledFormat> type() {
        return TYPE;
    }
}
