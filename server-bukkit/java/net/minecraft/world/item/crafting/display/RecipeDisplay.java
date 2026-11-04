// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.flag.FeatureFlagSet;
public interface RecipeDisplay {
    Codec<RecipeDisplay> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.display.RecipeDisplay.class, "CODEC");
    StreamCodec<RegistryFriendlyByteBuf, RecipeDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.RecipeDisplay.class, "STREAM_CODEC");
    SlotDisplay result();
    SlotDisplay craftingStation();
    Type<? extends RecipeDisplay> type();
    default boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return result().isEnabled(enabledFeatures) && craftingStation().isEnabled(enabledFeatures);
    }
    public static record Type<T extends RecipeDisplay>(MapCodec<T> codec, StreamCodec<RegistryFriendlyByteBuf, T> streamCodec) {
    }
}
