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
public record BrewingFuel(ResolvableInt uses, ResolvableFloat speedMultiplier) {
    public static final Codec<BrewingFuel> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.BrewingFuel.class, "CODEC");
    public static final StreamCodec<ByteBuf, BrewingFuel> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.BrewingFuel.class, "STREAM_CODEC");
    public BrewingFuel(ResourceKey<ContextIntProvider> uses, ResourceKey<ContextFloatProvider> speedMultiplier) {
        this(ResolvableInt.fromKey(uses), ResolvableFloat.fromKey(speedMultiplier));
    }
}
