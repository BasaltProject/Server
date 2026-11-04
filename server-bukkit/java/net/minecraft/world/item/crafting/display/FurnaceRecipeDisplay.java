// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.flag.FeatureFlagSet;
public record FurnaceRecipeDisplay(SlotDisplay ingredient, SlotDisplay fuel, SlotDisplay result, SlotDisplay craftingStation, int duration, float experience) implements RecipeDisplay {
    public static final MapCodec<FurnaceRecipeDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.FurnaceRecipeDisplay.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, FurnaceRecipeDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.FurnaceRecipeDisplay.class, "STREAM_CODEC");
    public static final RecipeDisplay.Type<FurnaceRecipeDisplay> TYPE = new RecipeDisplay.Type<>(FurnaceRecipeDisplay.MAP_CODEC, FurnaceRecipeDisplay.STREAM_CODEC);
    public RecipeDisplay.Type<FurnaceRecipeDisplay> type() {
        return TYPE;
    }
    public boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return ingredient.isEnabled(enabledFeatures) && fuel.isEnabled(enabledFeatures) && result.isEnabled(enabledFeatures) && craftingStation.isEnabled(enabledFeatures);
    }
}
