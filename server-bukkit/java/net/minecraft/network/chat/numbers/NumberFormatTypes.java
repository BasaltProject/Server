// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.numbers;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Registry;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class NumberFormatTypes {
    static final int BLANK_ID = 0;
    static final int STYLED_ID = 1;
    static final int FIXED_ID = 2;
    public static final MapCodec<NumberFormat> MAP_CODEC = net.minecraft.core.registries.BuiltInRegistries.NUMBER_FORMAT_TYPE
        .byNameCodec().dispatchMap(NumberFormat::type, NumberFormatType::mapCodec);
    public static final Codec<NumberFormat> CODEC = MAP_CODEC.codec();
    public static final StreamCodec<RegistryFriendlyByteBuf, NumberFormat> STREAM_CODEC = StreamCodec.of(
        (buf, format) -> format.write(buf), NumberFormatTypes::read);
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<NumberFormat>> OPTIONAL_STREAM_CODEC = STREAM_CODEC.apply(ByteBufCodecs::optional);

    public static NumberFormatType<?> bootstrap(Registry<NumberFormatType<?>> registry) {
        Registry.register(registry, "blank", BlankFormat.TYPE);
        Registry.register(registry, "styled", StyledFormat.TYPE);
        return Registry.register(registry, "fixed", FixedFormat.TYPE);
    }

    private static NumberFormat read(RegistryFriendlyByteBuf buf) {
        int id = buf.readVarInt();
        return switch (id) {
            case BLANK_ID -> BlankFormat.INSTANCE;
            case STYLED_ID -> StyledFormat.TYPE.streamCodec().decode(buf);
            case FIXED_ID -> FixedFormat.TYPE.streamCodec().decode(buf);
            default -> throw new io.netty.handler.codec.DecoderException("Unknown number format type " + id);
        };
    }
}
