// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt;
public record Compostable(ResolvableInt layers) {
    public static final Codec<Compostable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Compostable.class, "CODEC");
    public static final StreamCodec<ByteBuf, Compostable> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Compostable.class, "STREAM_CODEC");
    public Compostable(ResourceKey<ContextIntProvider> layers) {
        this(ResolvableInt.fromKey(layers));
    }
}
