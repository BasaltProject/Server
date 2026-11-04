// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.flag.FeatureFlagSet;
public record ShapedCraftingRecipeDisplay(int width, int height, List<SlotDisplay> ingredients, SlotDisplay result, SlotDisplay craftingStation) implements RecipeDisplay {
    public static final MapCodec<ShapedCraftingRecipeDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.ShapedCraftingRecipeDisplay.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ShapedCraftingRecipeDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.ShapedCraftingRecipeDisplay.class, "STREAM_CODEC");
    public static final RecipeDisplay.Type<ShapedCraftingRecipeDisplay> TYPE = new RecipeDisplay.Type<>(ShapedCraftingRecipeDisplay.MAP_CODEC, ShapedCraftingRecipeDisplay.STREAM_CODEC);
    public RecipeDisplay.Type<ShapedCraftingRecipeDisplay> type() {
        return TYPE;
    }
    public boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return ingredients.stream().allMatch(entry -> entry.isEnabled(enabledFeatures)) && result.isEnabled(enabledFeatures) && craftingStation.isEnabled(enabledFeatures);
    }
}
