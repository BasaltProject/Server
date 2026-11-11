// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public class BlastingRecipe extends AbstractCookingRecipe {
    public static final MapCodec<BlastingRecipe> MAP_CODEC = AbstractCookingRecipe.cookingMapCodec(BlastingRecipe::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, BlastingRecipe> STREAM_CODEC = AbstractCookingRecipe.cookingStreamCodec(BlastingRecipe::new);
    public static final RecipeSerializer<BlastingRecipe> SERIALIZER = new RecipeSerializer<>(MAP_CODEC, STREAM_CODEC);
    public BlastingRecipe(Recipe.CommonInfo commonInfo, AbstractCookingRecipe.CookingBookInfo bookInfo, Ingredient ingredient, ItemStackTemplate result, float experience, int cookingTime) {
        super(commonInfo, bookInfo, ingredient, result, experience, cookingTime);
    }
    protected Item furnaceIcon() {
        return net.minecraft.world.item.Items.BLAST_FURNACE;
    }
    public RecipeSerializer<BlastingRecipe> getSerializer() {
        return SERIALIZER;
    }
    public RecipeType<BlastingRecipe> getType() {
        return RecipeType.BLASTING;
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
