// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.item.crafting.display.SlotDisplay;
public class StonecutterRecipe extends SingleItemRecipe {
    public static final MapCodec<StonecutterRecipe> MAP_CODEC = SingleItemRecipe.simpleMapCodec(StonecutterRecipe::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, StonecutterRecipe> STREAM_CODEC = SingleItemRecipe.simpleStreamCodec(StonecutterRecipe::new);
    public static final RecipeSerializer<StonecutterRecipe> SERIALIZER = null;
    public StonecutterRecipe(Recipe.CommonInfo commonInfo, Ingredient ingredient, ItemStackTemplate result) {
        super(commonInfo, ingredient, result);
    }
    public RecipeType<StonecutterRecipe> getType() {
        return RecipeType.STONECUTTING;
    }
    public RecipeSerializer<StonecutterRecipe> getSerializer() {
        return SERIALIZER;
    }
    public String group() {
        return "";
    }
    public List<RecipeDisplay> display() {
        return List.of(new net.minecraft.world.item.crafting.display.StonecutterRecipeDisplay(input().display(),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay(result()),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay(net.minecraft.world.item.Items.STONECUTTER)));
    }
    public SlotDisplay resultDisplay() {
        return null;
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
