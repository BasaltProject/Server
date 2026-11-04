// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.chicken;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.ClientAsset;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.variant.ModelAndTexture;
import net.minecraft.world.entity.variant.PriorityProvider;
import net.minecraft.world.entity.variant.SpawnCondition;
import net.minecraft.world.entity.variant.SpawnContext;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
public record ChickenVariant(ModelAndTexture<ModelType> modelAndTexture, ClientAsset.ResourceTexture babyTexture, SpawnPrioritySelectors spawnConditions) implements PriorityProvider<SpawnContext, SpawnCondition> {
    public static final Codec<ChickenVariant> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.chicken.ChickenVariant.class, "DIRECT_CODEC");
    public static final Codec<ChickenVariant> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.chicken.ChickenVariant.class, "NETWORK_CODEC");
    public static final Codec<Holder<ChickenVariant>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.chicken.ChickenVariant.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<ChickenVariant>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.chicken.ChickenVariant.class, "STREAM_CODEC");
    private ChickenVariant(ModelAndTexture<ModelType> assetInfo, ClientAsset.ResourceTexture babyTexture) {
        this(assetInfo, babyTexture, SpawnPrioritySelectors.EMPTY);
    }
    public List<PriorityProvider.Selector<SpawnContext, SpawnCondition>> selectors() {
        return spawnConditions.selectors();
    }
    public static enum ModelType implements StringRepresentable {
        NORMAL, COLD;
        public static final Codec<ModelType> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.chicken.ChickenVariant.ModelType.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
