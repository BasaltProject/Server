// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment.trim;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
public record TrimPattern(Identifier assetId, Component description, boolean decal) {
    public static final Codec<TrimPattern> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.equipment.trim.TrimPattern.class, "DIRECT_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, TrimPattern> DIRECT_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.equipment.trim.TrimPattern.class, "DIRECT_STREAM_CODEC");
    public static final Codec<Holder<TrimPattern>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.equipment.trim.TrimPattern.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<TrimPattern>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.equipment.trim.TrimPattern.class, "STREAM_CODEC");
    public Component copyWithStyle(Holder<TrimMaterial> material) {
        return description.copy().withStyle(material.value().description().getStyle());
    }
}
