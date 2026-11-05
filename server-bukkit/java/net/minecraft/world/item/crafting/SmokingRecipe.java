// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public class SmokingRecipe extends AbstractCookingRecipe {
    public static final MapCodec<SmokingRecipe> MAP_CODEC = AbstractCookingRecipe.cookingMapCodec(SmokingRecipe::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, SmokingRecipe> STREAM_CODEC = AbstractCookingRecipe.cookingStreamCodec(SmokingRecipe::new);
    public static final RecipeSerializer<SmokingRecipe> SERIALIZER = null;
    public SmokingRecipe(Recipe.CommonInfo commonInfo, AbstractCookingRecipe.CookingBookInfo bookInfo, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime) {
        super(commonInfo, bookInfo, ingredient, result, experience, cookingTime);
    }
    protected Item furnaceIcon() {
        return net.minecraft.world.item.Items.SMOKER;
    }
    public RecipeType<SmokingRecipe> getType() {
        return RecipeType.SMOKING;
    }
    public RecipeSerializer<SmokingRecipe> getSerializer() {
        return SERIALIZER;
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
