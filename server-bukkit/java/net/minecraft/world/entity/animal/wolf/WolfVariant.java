// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.wolf;
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
public record WolfVariant(AssetInfo adultInfo, AssetInfo babyInfo, SpawnPrioritySelectors spawnConditions) implements PriorityProvider<SpawnContext, SpawnCondition> {
    public static final Codec<WolfVariant> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.wolf.WolfVariant.class, "DIRECT_CODEC");
    public static final Codec<WolfVariant> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.wolf.WolfVariant.class, "NETWORK_CODEC");
    public static final Codec<Holder<WolfVariant>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.wolf.WolfVariant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<WolfVariant>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.wolf.WolfVariant.class, "STREAM_CODEC");
    private WolfVariant(AssetInfo adultInfo, AssetInfo babyInfo) {
        this(adultInfo, babyInfo, SpawnPrioritySelectors.EMPTY);
    }
    public List<PriorityProvider.Selector<SpawnContext, SpawnCondition>> selectors() {
        return spawnConditions.selectors();
    }
    public static record AssetInfo(ClientAsset.ResourceTexture wild, ClientAsset.ResourceTexture tame, ClientAsset.ResourceTexture angry) {
        public static final Codec<AssetInfo> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.wolf.WolfVariant.AssetInfo.class, "CODEC");
    }
}
