// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.alchemy.Potion;

public abstract class BrewingProvider {
    private final RecipeOutput output;
    private final List<Item> containers = new ArrayList<>();
    private final List<ContainerTransformation> transformations = new ArrayList<>();
    private final List<Holder<Potion>> potions = new ArrayList<>();

    protected BrewingProvider(RecipeOutput output) {
        this.output = output;
    }

    protected void addContainerTransformation(Item container, Item reagent, Item output) {
        transformations.add(new ContainerTransformation(container, reagent, output));
    }

    protected void addContainer(Item container) {
        containers.add(container);
    }

    protected void buildMix(Holder<Potion> input, Item reagent, Holder<Potion> output) {
        remember(input);
        remember(output);
        for (Item container : containers) {
            save(BrewingRecipeBuilder.brewingMix(container, input, reagent, output));
        }
    }

    protected void buildStartMix(Item reagent, Holder<Potion> output) {
        buildMix(net.minecraft.world.item.alchemy.Potions.AWKWARD, reagent, output);
    }

    private void remember(Holder<Potion> potion) {
        if (!potions.contains(potion)) {
            potions.add(potion);
        }
    }

    protected void buildTransformations() {
        for (ContainerTransformation transformation : transformations) {
            for (Holder<Potion> potion : potions) {
                save(BrewingRecipeBuilder.brewingContainerTransform(transformation.container(), potion, transformation.reagent(), transformation.output()));
            }
        }
    }

    protected void save(BrewingRecipeBuilder builder) {
        builder.save(output);
    }

    public final void buildRecipes() {
        addContainers();
        addContainerTransformations();
        buildMixes();
        buildTransformations();
    }

    protected abstract void addContainers();

    protected abstract void addContainerTransformations();

    protected abstract void buildMixes();

    static record ContainerTransformation(Item container, Item reagent, Item output) {
    }
}
