// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageType;
public record DamageResistant(HolderSet<DamageType> types) {
    public static final Codec<DamageResistant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.DamageResistant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, DamageResistant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.DamageResistant.class, "STREAM_CODEC");
    public boolean isResistantTo(DamageSource source) {
        return types.contains(source.typeHolder());
    }
}
