// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public class SmeltingRecipe extends AbstractCookingRecipe {
    public static final MapCodec<SmeltingRecipe> MAP_CODEC = AbstractCookingRecipe.cookingMapCodec(SmeltingRecipe::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, SmeltingRecipe> STREAM_CODEC = AbstractCookingRecipe.cookingStreamCodec(SmeltingRecipe::new);
    public static final RecipeSerializer<SmeltingRecipe> SERIALIZER = null;
    public SmeltingRecipe(Recipe.CommonInfo commonInfo, AbstractCookingRecipe.CookingBookInfo bookInfo, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime) {
        super(commonInfo, bookInfo, ingredient, result, experience, cookingTime);
    }
    protected Item furnaceIcon() {
        return net.minecraft.world.item.Items.FURNACE;
    }
    public RecipeSerializer<SmeltingRecipe> getSerializer() {
        return SERIALIZER;
    }
    public RecipeType<SmeltingRecipe> getType() {
        return RecipeType.SMELTING;
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
