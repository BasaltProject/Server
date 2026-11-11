// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes.packs;

import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.advancements.Advancement;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.MultiRegistryBootstrap;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.recipes.RecipeBuilder;
import net.minecraft.data.recipes.RecipeProvider;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.equipment.trim.TrimPattern;

public class VanillaRecipeProvider extends RecipeProvider {
    private static final String[] kTrimNames = {"sentry", "dune", "coast", "wild", "ward", "eye", "vex", "tide", "snout", "rib",
        "spire", "wayfinder", "shaper", "silence", "raiser", "host", "flow", "bolt"};

    public VanillaRecipeProvider(BootstrapContext<Recipe<?>> recipeOutput, BootstrapContext<Advancement> advancementOutput) {
        super(recipeOutput, advancementOutput);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static MultiRegistryBootstrap create() {
        return new MultiRegistryBootstrap() {
            @Override
            public Set<ResourceKey<? extends net.minecraft.core.Registry<?>>> requestedRegistries() {
                return Set.of((ResourceKey) Registries.RECIPE, (ResourceKey) Registries.ADVANCEMENT);
            }

            @Override
            public void run(BootstrapGetter registries) {
                new VanillaRecipeProvider(registries.get((ResourceKey) Registries.RECIPE),
                    registries.get((ResourceKey) Registries.ADVANCEMENT)).buildRecipes();
            }
        };
    }

    @Override
    protected void buildRecipes() {
        generateForEnabledBlockFamilies(FeatureFlagSet.of());
    }

    public static Stream<TrimTemplate> smithingTrims() {
        return Stream.of(kTrimNames).map(name -> {
            Identifier templateId = Identifier.withDefaultNamespace(name + "_armor_trim_smithing_template");
            Item template = BuiltInRegistries.ITEM.getValue(templateId);
            ResourceKey<TrimPattern> pattern = ResourceKey.create(Registries.TRIM_PATTERN, Identifier.withDefaultNamespace(name));
            return new TrimTemplate(template, pattern, RecipeBuilder.recipeKey(templateId.withSuffix("_smithing_trim")));
        });
    }

    public static record TrimTemplate(Item template, ResourceKey<TrimPattern> patternId, ResourceKey<Recipe<?>> recipeId) {
    }
}
