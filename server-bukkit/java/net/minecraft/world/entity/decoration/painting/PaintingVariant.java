// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration.painting;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
public record PaintingVariant(int width, int height, Identifier assetId, Optional<Component> title, Optional<Component> author) {
    public static final Codec<PaintingVariant> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.decoration.painting.PaintingVariant.class, "DIRECT_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PaintingVariant> DIRECT_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.decoration.painting.PaintingVariant.class, "DIRECT_STREAM_CODEC");
    public static final Codec<Holder<PaintingVariant>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.decoration.painting.PaintingVariant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<PaintingVariant>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.decoration.painting.PaintingVariant.class, "STREAM_CODEC");
    public int area() {
        return width * height;
    }
}
