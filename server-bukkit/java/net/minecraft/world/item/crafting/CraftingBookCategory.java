// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.IntFunction;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
public enum CraftingBookCategory implements StringRepresentable {
    BUILDING, REDSTONE, EQUIPMENT, MISC;
    public static final Codec<CraftingBookCategory> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.CraftingBookCategory.class, "CODEC");
    public static final IntFunction<CraftingBookCategory> BY_ID = net.minecraft.util.ByIdMap.continuous(CraftingBookCategory::ordinal, values(), net.minecraft.util.ByIdMap.OutOfBoundsStrategy.ZERO);
    public static final StreamCodec<ByteBuf, CraftingBookCategory> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.CraftingBookCategory.class, "STREAM_CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
