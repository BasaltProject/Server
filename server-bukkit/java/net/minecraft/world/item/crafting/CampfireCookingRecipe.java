// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public class CampfireCookingRecipe extends AbstractCookingRecipe {
    public static final MapCodec<CampfireCookingRecipe> MAP_CODEC = AbstractCookingRecipe.cookingMapCodec(CampfireCookingRecipe::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, CampfireCookingRecipe> STREAM_CODEC = AbstractCookingRecipe.cookingStreamCodec(CampfireCookingRecipe::new);
    public static final RecipeSerializer<CampfireCookingRecipe> SERIALIZER = null;
    public CampfireCookingRecipe(Recipe.CommonInfo commonInfo, AbstractCookingRecipe.CookingBookInfo bookInfo, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime) {
        super(commonInfo, bookInfo, ingredient, result, experience, cookingTime);
    }
    protected Item furnaceIcon() {
        return net.minecraft.world.item.Items.CAMPFIRE;
    }
    public RecipeSerializer<CampfireCookingRecipe> getSerializer() {
        return SERIALIZER;
    }
    public RecipeType<CampfireCookingRecipe> getType() {
        return RecipeType.CAMPFIRE_COOKING;
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
