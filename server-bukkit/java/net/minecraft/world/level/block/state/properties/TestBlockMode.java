// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
public enum TestBlockMode implements StringRepresentable {
    START, LOG, FAIL, ACCEPT;
    public static final Codec<TestBlockMode> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.properties.TestBlockMode.class, "CODEC");
    public static final StreamCodec<ByteBuf, TestBlockMode> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.state.properties.TestBlockMode.class, "STREAM_CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public Component getDisplayName() {
        return Component.translatable("test_block.mode." + getSerializedName());
    }
    public Component getDetailedMessage() {
        return Component.translatable("test_block.mode_info." + getSerializedName());
    }
}
