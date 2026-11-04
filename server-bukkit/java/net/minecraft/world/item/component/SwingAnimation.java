// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.SwingAnimationType;
public record SwingAnimation(SwingAnimationType type, int duration) {
    public static final SwingAnimation DEFAULT = new SwingAnimation(net.minecraft.world.item.SwingAnimationType.WHACK, 6);
    public static final Codec<SwingAnimation> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.SwingAnimation.class, "CODEC");
    public static final StreamCodec<ByteBuf, SwingAnimation> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.SwingAnimation.class, "STREAM_CODEC");
}
