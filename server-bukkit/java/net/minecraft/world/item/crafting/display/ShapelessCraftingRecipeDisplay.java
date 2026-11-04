// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.flag.FeatureFlagSet;
public record ShapelessCraftingRecipeDisplay(List<SlotDisplay> ingredients, SlotDisplay result, SlotDisplay craftingStation) implements RecipeDisplay {
    public static final MapCodec<ShapelessCraftingRecipeDisplay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.display.ShapelessCraftingRecipeDisplay.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ShapelessCraftingRecipeDisplay> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.display.ShapelessCraftingRecipeDisplay.class, "STREAM_CODEC");
    public static final RecipeDisplay.Type<ShapelessCraftingRecipeDisplay> TYPE = new RecipeDisplay.Type<>(ShapelessCraftingRecipeDisplay.MAP_CODEC, ShapelessCraftingRecipeDisplay.STREAM_CODEC);
    public RecipeDisplay.Type<ShapelessCraftingRecipeDisplay> type() {
        return TYPE;
    }
    public boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return ingredients.stream().allMatch(entry -> entry.isEnabled(enabledFeatures)) && result.isEnabled(enabledFeatures) && craftingStation.isEnabled(enabledFeatures);
    }
}
