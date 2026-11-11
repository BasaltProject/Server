// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.floats.ResolvableFloat;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ResolvableInt;
public record CookingFuel(ResolvableInt burnTime, ResolvableFloat speedMultiplier) {
    public static final Codec<CookingFuel> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.CookingFuel.class, "CODEC");
    public static final StreamCodec<ByteBuf, CookingFuel> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.CookingFuel.class, "STREAM_CODEC");
    public CookingFuel(ResourceKey<ContextIntProvider> burnTime, ResourceKey<ContextFloatProvider> speedMultiplier) {
        this(ResolvableInt.fromKey(burnTime), ResolvableFloat.fromKey(speedMultiplier));
    }
}
