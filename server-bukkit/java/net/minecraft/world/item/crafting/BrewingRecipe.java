// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.level.Level;
public class BrewingRecipe implements Recipe<BrewingInput> {
    public static final MapCodec<BrewingRecipe> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(
        PotionIngredient.MAP_CODEC.fieldOf("input").forGetter(o -> o.input),
        PotionIngredient.MAP_CODEC.fieldOf("reagent").forGetter(o -> o.reagent),
        ItemStackTemplate.CODEC.fieldOf("output").forGetter(o -> o.output)).apply(i, BrewingRecipe::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, BrewingRecipe> STREAM_CODEC = StreamCodec.composite(
        PotionIngredient.STREAM_CODEC, o -> o.input,
        PotionIngredient.STREAM_CODEC, o -> o.reagent,
        ItemStackTemplate.STREAM_CODEC, o -> o.output,
        BrewingRecipe::new);
    public static final RecipeSerializer<BrewingRecipe> SERIALIZER = null;
    private final Recipe.CommonInfo commonInfo = new Recipe.CommonInfo(false);
    private final PotionIngredient input;
    private final PotionIngredient reagent;
    private final ItemStackTemplate output;

    public BrewingRecipe(PotionIngredient input, PotionIngredient reagent, ItemStackTemplate output) {
        this.input = input;
        this.reagent = reagent;
        this.output = output;
    }
    public PotionIngredient getInput() {
        return input;
    }
    public PotionIngredient getReagent() {
        return reagent;
    }
    public ItemStackTemplate getOutput() {
        return output;
    }
    public boolean matches(BrewingInput brewingInput, Level level) {
        return matches(brewingInput);
    }
    public boolean matches(BrewingInput brewingInput) {
        return input.test(brewingInput.input()) && reagent.test(brewingInput.reagent());
    }
    public ItemStack assemble(BrewingInput input) {
        return output.create();
    }
    public RecipeType<BrewingRecipe> getType() {
        return RecipeType.BREWING;
    }
    public PlacementInfo placementInfo() {
        return PlacementInfo.NOT_PLACEABLE;
    }
    public boolean isSpecial() {
        return true;
    }
    public boolean showNotification() {
        return false;
    }
    public String group() {
        return "";
    }
    public RecipeBookCategory recipeBookCategory() {
        return null;
    }
    public RecipeSerializer<BrewingRecipe> getSerializer() {
        return SERIALIZER;
    }
    public interface Factory<T extends BrewingRecipe> {
        T create(Recipe.CommonInfo commonInfo, PotionIngredient input, PotionIngredient reagent, ItemStackTemplate output);
    }
}
