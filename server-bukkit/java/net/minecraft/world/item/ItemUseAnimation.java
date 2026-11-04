// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
public enum ItemUseAnimation implements StringRepresentable {
    NONE, EAT, DRINK, BLOCK, BOW, TRIDENT, CROSSBOW, SPYGLASS, TOOT_HORN, BRUSH, BUNDLE, SPEAR;
    public static final Codec<ItemUseAnimation> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.ItemUseAnimation.class, "CODEC");
    public static final StreamCodec<ByteBuf, ItemUseAnimation> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.ItemUseAnimation.class, "STREAM_CODEC");
    public int getId() {
        return ordinal();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public boolean hasCustomArmTransform() {
        return this == EAT || this == DRINK || this == SPEAR;
    }
}
