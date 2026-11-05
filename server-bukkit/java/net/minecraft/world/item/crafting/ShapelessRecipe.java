// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.display.RecipeDisplay;
import net.minecraft.world.level.Level;
public class ShapelessRecipe extends NormalCraftingRecipe {
    public static final MapCodec<ShapelessRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        Recipe.CommonInfo.MAP_CODEC.forGetter(o -> o.commonInfo),
        CraftingRecipe.CraftingBookInfo.MAP_CODEC.forGetter(o -> o.bookInfo),
        ItemStackTemplate.CODEC.fieldOf("result").forGetter(o -> o.result),
        Ingredient.CODEC.listOf(1, 9).fieldOf("ingredients").forGetter(o -> o.ingredients)).apply(i, ShapelessRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, ShapelessRecipe> STREAM_CODEC = StreamCodec.composite(
        Recipe.CommonInfo.STREAM_CODEC, o -> o.commonInfo,
        CraftingRecipe.CraftingBookInfo.STREAM_CODEC, o -> o.bookInfo,
        ItemStackTemplate.STREAM_CODEC, o -> o.result,
        Ingredient.CONTENTS_STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs.list()), o -> o.ingredients,
        ShapelessRecipe::new);
    public static final RecipeSerializer<ShapelessRecipe> SERIALIZER = null;
    private final ItemStackTemplate result;
    private final List<Ingredient> ingredients;

    public ShapelessRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo, ItemStackTemplate result, List<Ingredient> ingredients) {
        super(commonInfo, bookInfo);
        this.result = result;
        this.ingredients = ingredients;
    }
    public RecipeSerializer<ShapelessRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return PlacementInfo.create(ingredients);
    }
    public boolean matches(CraftingInput input, Level level) {
        if (input.ingredientCount() != ingredients.size()) {
            return false;
        }
        java.util.List<ItemStack> stacks = new java.util.ArrayList<>();
        for (ItemStack stack : input.items()) {
            if (!stack.isEmpty()) {
                stacks.add(stack);
            }
        }
        boolean[] used = new boolean[stacks.size()];
        return assign(0, stacks, used);
    }
    public ItemStack assemble(CraftingInput input) {
        return result.create();
    }
    public List<RecipeDisplay> display() {
        return List.of(new net.minecraft.world.item.crafting.display.ShapelessCraftingRecipeDisplay(ingredients.stream().map(Ingredient::display).toList(),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay(result),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay(net.minecraft.world.item.Items.CRAFTING_TABLE)));
    }

    private boolean assign(int index, List<ItemStack> stacks, boolean[] used) {
        if (index == ingredients.size()) {
            return true;
        }
        for (int i = 0; i < stacks.size(); i++) {
            if (!used[i] && ingredients.get(index).test(stacks.get(i))) {
                used[i] = true;
                if (assign(index + 1, stacks, used)) {
                    return true;
                }
                used[i] = false;
            }
        }
        return false;
    }
}
