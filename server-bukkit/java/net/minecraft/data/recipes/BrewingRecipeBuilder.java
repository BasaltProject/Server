// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import java.util.Optional;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.Holder;
import net.minecraft.core.component.predicates.PotionsPredicate;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.item.crafting.Recipe;

public class BrewingRecipeBuilder implements RecipeBuilder {
    private final Item inputItem;
    private final Holder<Potion> inputPotion;
    private final Item reagent;
    private final Item outputItem;
    private final Holder<Potion> outputPotion;

    private BrewingRecipeBuilder(Item inputItem, Holder<Potion> inputPotion, Item reagent, Item outputItem, Holder<Potion> outputPotion) {
        this.inputItem = inputItem;
        this.inputPotion = inputPotion;
        this.reagent = reagent;
        this.outputItem = outputItem;
        this.outputPotion = outputPotion;
    }

    public static BrewingRecipeBuilder brewingMix(Item container, Holder<Potion> inputPotion, Item reagentItem, Holder<Potion> outputPotion) {
        return new BrewingRecipeBuilder(container, inputPotion, reagentItem, container, outputPotion);
    }

    public static BrewingRecipeBuilder brewingContainerTransform(Item inputContainer, Holder<Potion> inputPotion, Item reagentItem, Item outputContainer) {
        return new BrewingRecipeBuilder(inputContainer, inputPotion, reagentItem, outputContainer, inputPotion);
    }

    @Override
    public RecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public BrewingRecipeBuilder group(String group) {
        return this;
    }

    public static Optional<Holder<Potion>> getExactPotion(PotionsPredicate predicate) {
        return Optional.empty();
    }

    private static String potionName(Holder<Potion> potion) {
        return potion.unwrapKey().map(key -> key.identifier().toString()).orElse("minecraft:water");
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        String potion = Identifier.parse(potionName(inputPotion)).getPath();
        String path = "brewing/" + RecipeJson.itemId(inputItem).getPath() + "_" + potion + "_" + RecipeJson.itemId(reagent).getPath();
        return RecipeBuilder.recipeKey(Identifier.withDefaultNamespace(path));
    }

    @Override
    public void save(RecipeOutput recipeOutput, ResourceKey<Recipe<?>> id) {
        JsonObject input = new JsonObject();
        input.addProperty("item", RecipeJson.itemId(inputItem).toString());
        input.addProperty("potion_contents", potionName(inputPotion));
        JsonObject reagentJson = new JsonObject();
        reagentJson.addProperty("item", RecipeJson.itemId(reagent).toString());
        JsonObject potion = new JsonObject();
        potion.addProperty("potion", potionName(outputPotion));
        JsonObject components = new JsonObject();
        components.add("minecraft:potion_contents", potion);
        JsonObject output = new JsonObject();
        output.addProperty("id", RecipeJson.itemId(outputItem).toString());
        output.add("components", components);
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:brewing");
        json.add("input", input);
        json.add("reagent", reagentJson);
        json.add("output", output);
        RecipeJson.emit(recipeOutput, id.identifier(), json);
    }
}
