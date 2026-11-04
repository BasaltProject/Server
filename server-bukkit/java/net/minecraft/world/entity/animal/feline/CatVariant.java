// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.feline;
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
public record CatVariant(ClientAsset.ResourceTexture adultAssetInfo, ClientAsset.ResourceTexture babyAssetInfo, SpawnPrioritySelectors spawnConditions) implements PriorityProvider<SpawnContext, SpawnCondition> {
    public static final Codec<CatVariant> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.feline.CatVariant.class, "DIRECT_CODEC");
    public static final Codec<CatVariant> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.feline.CatVariant.class, "NETWORK_CODEC");
    public static final Codec<Holder<CatVariant>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.feline.CatVariant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<CatVariant>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.feline.CatVariant.class, "STREAM_CODEC");
    private CatVariant(ClientAsset.ResourceTexture adultAssetInfo, ClientAsset.ResourceTexture babyAssetInfo) {
        this(adultAssetInfo, babyAssetInfo, SpawnPrioritySelectors.EMPTY);
    }
    public List<PriorityProvider.Selector<SpawnContext, SpawnCondition>> selectors() {
        return spawnConditions.selectors();
    }
    public ClientAsset.ResourceTexture assetInfo(boolean isBaby) {
        return isBaby ? babyAssetInfo : adultAssetInfo;
    }
}
