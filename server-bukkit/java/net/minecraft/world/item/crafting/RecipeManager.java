// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.Collection;
import java.util.Map;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.HolderLookup;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.crafting.display.RecipeDisplayEntry;
import net.minecraft.world.item.crafting.display.RecipeDisplayId;
import net.minecraft.world.level.Level;
public class RecipeManager implements RecipeAccess {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static RecipeHolder<?> holder(net.minecraft.resources.Identifier id) {
        return new RecipeHolder(net.minecraft.resources.ResourceKey.create(
            net.minecraft.core.registries.Registries.RECIPE, id), new org.basaltmc.bukkit.facade.NativeCraftingRecipe(id));
    }

    public RecipeManager(HolderLookup.Provider registries) {

    }
    public void finalizeRecipeLoading(FeatureFlagSet enabledFlags) {

    }
    public <I extends RecipeInput, T extends Recipe<I>> Optional<RecipeHolder<T>> getRecipeFor(RecipeType<T> type, I input, Level level, ResourceKey<Recipe<?>> recipeHint) {
        return getRecipeFor(type, input, level);
    }
    public <I extends RecipeInput, T extends Recipe<I>> Optional<RecipeHolder<T>> getRecipeFor(RecipeType<T> type, I input, Level level, RecipeHolder<T> recipeHint) {
        return getRecipeFor(type, input, level);
    }
    @SuppressWarnings("unchecked")
    public <I extends RecipeInput, T extends Recipe<I>> Optional<RecipeHolder<T>> getRecipeFor(RecipeType<T> type, I input, Level level) {
        if (type != RecipeType.CRAFTING || !(input instanceof CraftingInput crafting) || crafting.isEmpty()) {
            return Optional.empty();
        }
        org.basaltmc.bukkit.recipe.Recipes.Crafted crafted = org.basaltmc.bukkit.facade.NativeCraftingRecipe.craftPublic(crafting);
        if (crafted == null || crafted.recipe() == null) {
            return Optional.empty();
        }
        net.minecraft.resources.Identifier id = net.minecraft.resources.Identifier.parse(crafted.recipe().toString());
        return Optional.of((RecipeHolder<T>) holder(id));
    }
    public Optional<RecipeHolder<?>> byKey(ResourceKey<Recipe<?>> recipeId) {
        String text = recipeId.identifier().toString();
        for (net.minecraft.resources.Identifier id : org.basaltmc.bukkit.facade.NativeCraftingRecipe.allIds()) {
            if (id.toString().equals(text)) {
                return Optional.of(holder(id));
            }
        }
        return Optional.empty();
    }
    public Map<ResourceKey<RecipePropertySet>, RecipePropertySet> getSynchronizedItemProperties() {
        return Map.of();
    }
    public SelectableRecipe.SingleInputSet<StonecutterRecipe> getSynchronizedStonecutterRecipes() {
        return SelectableRecipe.SingleInputSet.empty();
    }
    public RecipePropertySet propertySet(ResourceKey<RecipePropertySet> id) {
        return null;
    }
    public SelectableRecipe.SingleInputSet<StonecutterRecipe> stonecutterRecipes() {
        return SelectableRecipe.SingleInputSet.empty();
    }
    public Collection<RecipeHolder<?>> getRecipes() {
        java.util.List<RecipeHolder<?>> all = new java.util.ArrayList<>();
        for (net.minecraft.resources.Identifier id : org.basaltmc.bukkit.facade.NativeCraftingRecipe.allIds()) {
            all.add(holder(id));
        }
        return all;
    }
    public Collection<RecipeHolder<?>> getLearnableRecipes() {
        return getRecipes();
    }
    public ServerDisplayInfo getRecipeFromDisplay(RecipeDisplayId id) {
        return null;
    }
    public void listDisplaysForRecipe(ResourceKey<Recipe<?>> id, Consumer<RecipeDisplayEntry> output) {

    }
    public static <I extends RecipeInput, T extends Recipe<I>> CachedCheck<I, T> createCheck(RecipeType<T> type) {
        return (input, level) -> level.recipeAccess().getRecipeFor(type, input, level);
    }
    public static record ServerDisplayInfo(RecipeDisplayEntry display, RecipeHolder<?> parent) {
    }
    public static class IngredientCollector implements Consumer<Recipe<?>> {
        private final ResourceKey<RecipePropertySet> key;
        private final IngredientExtractor extractor;
        private final java.util.List<Ingredient> ingredients = new java.util.ArrayList<>();
        protected IngredientCollector(ResourceKey<RecipePropertySet> key, IngredientExtractor extractor) {
            this.key = key;
            this.extractor = extractor;
        }
        public void accept(Recipe<?> recipe) {
            extractor.apply(recipe).ifPresent(ingredients::add);
        }
        public RecipePropertySet asPropertySet(FeatureFlagSet enabledFeatures) {
            return RecipePropertySet.create(ingredients);
        }
    }
    public interface CachedCheck<I extends RecipeInput, T extends Recipe<I>> {
        Optional<RecipeHolder<T>> getRecipeFor(I input, ServerLevel level);
    }
    public interface IngredientExtractor {
        Optional<Ingredient> apply(Recipe<?> recipe);
    }
}
