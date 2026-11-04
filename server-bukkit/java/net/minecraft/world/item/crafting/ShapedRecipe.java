// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.level.Level;
public class ShapedRecipe extends NormalCraftingRecipe {
    public static final MapCodec<ShapedRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.ShapedRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ShapedRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.ShapedRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<ShapedRecipe> SERIALIZER = null;
    private final ShapedRecipePattern pattern;
    private final ItemStackTemplate result;

    public ShapedRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo, ShapedRecipePattern pattern, ItemStackTemplate result) {
        super(commonInfo, bookInfo);
        this.pattern = pattern;
        this.result = result;
    }
    public RecipeSerializer<ShapedRecipe> getSerializer() {
        return SERIALIZER;
    }
    public List<Optional<Ingredient>> getIngredients() {
        return pattern.ingredients();
    }
    protected PlacementInfo createPlacementInfo() {
        return PlacementInfo.createFromOptionals(pattern.ingredients());
    }
    public boolean matches(CraftingInput input, Level level) {
        return pattern.matches(input);
    }
    public ItemStack assemble(CraftingInput input) {
        return result.create();
    }
    public int getWidth() {
        return pattern.width();
    }
    public int getHeight() {
        return pattern.height();
    }
    public List<RecipeDisplay> display() {
        List<net.minecraft.world.item.crafting.display.SlotDisplay> slots = pattern.ingredients().stream().map(Ingredient::optionalIngredientToDisplay).toList();
        return List.of(new net.minecraft.world.item.crafting.display.ShapedCraftingRecipeDisplay(pattern.width(), pattern.height(), slots,
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay(result),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay(net.minecraft.world.item.Items.CRAFTING_TABLE)));
    }
}
