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
public class SmithingTransformRecipe extends SimpleSmithingRecipe {
    public static final MapCodec<SmithingTransformRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.SmithingTransformRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, SmithingTransformRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.SmithingTransformRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<SmithingTransformRecipe> SERIALIZER = null;
    private final Optional<Ingredient> template;
    private final Ingredient base;
    private final Optional<Ingredient> addition;
    private final ItemStackTemplate result;

    public SmithingTransformRecipe(Recipe.CommonInfo commonInfo, Optional<Ingredient> template, Ingredient base, Optional<Ingredient> addition, ItemStackTemplate result) {
        super(commonInfo);
        this.template = template;
        this.base = base;
        this.addition = addition;
        this.result = result;
    }
    public ItemStack assemble(SmithingRecipeInput input) {
        ItemStack out = result.create();
        out.applyComponents(input.base().getComponentsPatch());
        return out;
    }
    public Optional<Ingredient> templateIngredient() {
        return template;
    }
    public Ingredient baseIngredient() {
        return base;
    }
    public Optional<Ingredient> additionIngredient() {
        return addition;
    }
    public RecipeSerializer<SmithingTransformRecipe> getSerializer() {
        return SERIALIZER;
    }
    protected PlacementInfo createPlacementInfo() {
        return PlacementInfo.createFromOptionals(List.of(template, Optional.of(base), addition));
    }
    public List<RecipeDisplay> display() {
        return List.of(new net.minecraft.world.item.crafting.display.SmithingRecipeDisplay(Ingredient.optionalIngredientToDisplay(template), base.display(),
            Ingredient.optionalIngredientToDisplay(addition), new net.minecraft.world.item.crafting.display.SlotDisplay.ItemStackSlotDisplay(result),
            new net.minecraft.world.item.crafting.display.SlotDisplay.ItemSlotDisplay(net.minecraft.world.item.Items.SMITHING_TABLE)));
    }
}
