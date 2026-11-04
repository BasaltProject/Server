// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public interface FrogVariants {
    ResourceKey<FrogVariant> TEMPERATE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.FROG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("temperate"));
    ResourceKey<FrogVariant> WARM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.FROG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("warm"));
    ResourceKey<FrogVariant> COLD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.FROG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cold"));
    static void bootstrap(BootstrapContext<FrogVariant> registry) {
        register(registry, TEMPERATE, "entity/frog/frog_temperate", SpawnPrioritySelectors.fallback(0));
        register(registry, WARM, "entity/frog/frog_warm", net.minecraft.tags.BiomeTags.SPAWNS_WARM_VARIANT_FROGS);
        register(registry, COLD, "entity/frog/frog_cold", net.minecraft.tags.BiomeTags.SPAWNS_COLD_VARIANT_FROGS);
    }
    private static void register(BootstrapContext<FrogVariant> context, ResourceKey<FrogVariant> name, String assetId,
            net.minecraft.tags.TagKey<net.minecraft.world.level.biome.Biome> limitToBiome) {
        net.minecraft.core.HolderSet<net.minecraft.world.level.biome.Biome> biomes = context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(limitToBiome);
        register(context, name, assetId, SpawnPrioritySelectors.single(new BiomeCheck(biomes), 1));
    }
    private static void register(BootstrapContext<FrogVariant> context, ResourceKey<FrogVariant> name, String assetId, SpawnPrioritySelectors selectors) {
        context.register(name, new FrogVariant(new ClientAsset.ResourceTexture(net.minecraft.resources.Identifier.withDefaultNamespace(assetId)), selectors));
    }
}
