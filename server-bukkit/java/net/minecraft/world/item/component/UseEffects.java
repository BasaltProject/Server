// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record UseEffects(boolean canSprint, boolean interactVibrations, float speedMultiplier) {
    public static final UseEffects DEFAULT = new UseEffects(true, true, 1.0F);
    public static final Codec<UseEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.UseEffects.class, "CODEC");
    public static final StreamCodec<ByteBuf, UseEffects> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.UseEffects.class, "STREAM_CODEC");
}
