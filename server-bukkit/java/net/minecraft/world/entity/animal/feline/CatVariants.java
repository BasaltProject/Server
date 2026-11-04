// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.feline;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.MoonBrightnessCheck;
import net.minecraft.world.entity.variant.PriorityProvider;
import net.minecraft.world.entity.variant.SpawnCondition;
import net.minecraft.world.entity.variant.SpawnContext;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.world.entity.variant.StructureCheck;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public interface CatVariants {
    ResourceKey<CatVariant> TABBY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("tabby"));
    ResourceKey<CatVariant> BLACK = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("black"));
    ResourceKey<CatVariant> RED = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("red"));
    ResourceKey<CatVariant> SIAMESE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("siamese"));
    ResourceKey<CatVariant> BRITISH_SHORTHAIR = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("british_shorthair"));
    ResourceKey<CatVariant> CALICO = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("calico"));
    ResourceKey<CatVariant> PERSIAN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("persian"));
    ResourceKey<CatVariant> RAGDOLL = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("ragdoll"));
    ResourceKey<CatVariant> WHITE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("white"));
    ResourceKey<CatVariant> JELLIE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("jellie"));
    ResourceKey<CatVariant> ALL_BLACK = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CAT_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("all_black"));
    static void bootstrap(BootstrapContext<CatVariant> context) {
        for (ResourceKey<CatVariant> key : java.util.List.of(TABBY, BLACK, RED, SIAMESE, BRITISH_SHORTHAIR, CALICO, PERSIAN, RAGDOLL, WHITE, JELLIE)) {
            String name = key.identifier().getPath();
            register(context, key, "entity/cat/cat_" + name, "entity/cat/cat_" + name + "_baby", SpawnPrioritySelectors.fallback(0));
        }
        @SuppressWarnings({"unchecked", "rawtypes"}) net.minecraft.core.HolderGetter<net.minecraft.world.level.levelgen.structure.Structure> structures = (net.minecraft.core.HolderGetter) context.lookup(net.minecraft.core.registries.Registries.STRUCTURE);
        register(context, ALL_BLACK, "entity/cat/cat_all_black", "entity/cat/cat_all_black_baby", new SpawnPrioritySelectors(java.util.List.of(
            new PriorityProvider.Selector<SpawnContext, SpawnCondition>(new StructureCheck(structures.getOrThrow(net.minecraft.tags.StructureTags.CATS_SPAWN_AS_BLACK)), 1),
            new PriorityProvider.Selector<SpawnContext, SpawnCondition>(new MoonBrightnessCheck(net.minecraft.advancements.predicates.MinMaxBounds.Doubles.atLeast(0.9)), 0))));
    }
    private static void register(BootstrapContext<CatVariant> context, ResourceKey<CatVariant> name, String adultTexture, String babyTexture, SpawnPrioritySelectors spawnConditions) {
        context.register(name, new CatVariant(new ClientAsset.ResourceTexture(net.minecraft.resources.Identifier.withDefaultNamespace(adultTexture)),
            new ClientAsset.ResourceTexture(net.minecraft.resources.Identifier.withDefaultNamespace(babyTexture)), spawnConditions));
    }
}
