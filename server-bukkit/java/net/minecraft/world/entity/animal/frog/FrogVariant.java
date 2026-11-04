// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.ClientAsset;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.variant.PriorityProvider;
import net.minecraft.world.entity.variant.SpawnCondition;
import net.minecraft.world.entity.variant.SpawnContext;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
public record FrogVariant(ClientAsset.ResourceTexture assetInfo, SpawnPrioritySelectors spawnConditions) implements PriorityProvider<SpawnContext, SpawnCondition> {
    public static final Codec<FrogVariant> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.frog.FrogVariant.class, "DIRECT_CODEC");
    public static final Codec<FrogVariant> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.frog.FrogVariant.class, "NETWORK_CODEC");
    public static final Codec<Holder<FrogVariant>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.frog.FrogVariant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<FrogVariant>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.frog.FrogVariant.class, "STREAM_CODEC");
    private FrogVariant(ClientAsset.ResourceTexture assetInfo) {
        this(assetInfo, SpawnPrioritySelectors.EMPTY);
    }
    public List<PriorityProvider.Selector<SpawnContext, SpawnCondition>> selectors() {
        return spawnConditions.selectors();
    }
}
