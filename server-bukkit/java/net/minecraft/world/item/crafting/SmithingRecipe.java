// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.Optional;
import net.minecraft.world.level.Level;
public interface SmithingRecipe extends Recipe<SmithingRecipeInput> {
    default RecipeType<SmithingRecipe> getType() {
        return RecipeType.SMITHING;
    }
    RecipeSerializer<? extends SmithingRecipe> getSerializer();
    default boolean matches(SmithingRecipeInput input, Level level) {
        return Ingredient.testOptionalIngredient(templateIngredient(), input.template()) && baseIngredient().test(input.base())
            && Ingredient.testOptionalIngredient(additionIngredient(), input.addition());
    }
    Optional<Ingredient> templateIngredient();
    Ingredient baseIngredient();
    Optional<Ingredient> additionIngredient();
    default RecipeBookCategory recipeBookCategory() {
        return null;
    }
}
