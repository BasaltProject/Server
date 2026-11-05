// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
public enum ChatVisiblity {
    FULL, SYSTEM, HIDDEN;
;
    public static final Codec<ChatVisiblity> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.player.ChatVisiblity.class, "LEGACY_CODEC");
    public static final StreamCodec<ByteBuf, ChatVisiblity> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.player.ChatVisiblity.class, "STREAM_CODEC");
    public Component caption() {
        return Component.translatable("options.chat.visibility." + name().toLowerCase(java.util.Locale.ROOT));
    }
}
