// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public class SmokingRecipe extends AbstractCookingRecipe {
    public static final MapCodec<SmokingRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.SmokingRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, SmokingRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.SmokingRecipe.class, "STREAM_CODEC");
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
