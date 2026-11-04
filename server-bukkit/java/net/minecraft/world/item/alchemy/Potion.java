// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.alchemy;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.flag.FeatureFlag;
import net.minecraft.world.flag.FeatureFlagSet;
public class Potion implements FeatureElement {
    public static final Codec<Holder<Potion>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.alchemy.Potion.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<Potion>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.alchemy.Potion.class, "STREAM_CODEC");
    private final String name;
    private final List<MobEffectInstance> effects;
    private FeatureFlagSet requiredFeatures = FeatureFlagSet.of();
    public Potion(String name, MobEffectInstance... effects) {
        this.name = name;
        this.effects = List.of(effects);
    }
    public Potion requiredFeatures(FeatureFlag... flags) {
        return this;
    }
    public FeatureFlagSet requiredFeatures() {
        return requiredFeatures;
    }
    public List<MobEffectInstance> getEffects() {
        return effects;
    }
    public String name() {
        return name;
    }
    public boolean hasInstantEffects() {
        for (MobEffectInstance effect : effects) {
            if (effect.getEffect().value().isInstantaneous()) {
                return true;
            }
        }
        return false;
    }
}
