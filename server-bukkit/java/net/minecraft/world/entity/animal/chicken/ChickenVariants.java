// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.chicken;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.ModelAndTexture;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class ChickenVariants {
    public static final ResourceKey<ChickenVariant> TEMPERATE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CHICKEN_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("temperate"));
    public static final ResourceKey<ChickenVariant> WARM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CHICKEN_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("warm"));
    public static final ResourceKey<ChickenVariant> COLD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CHICKEN_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cold"));
    public static final ResourceKey<ChickenVariant> DEFAULT = TEMPERATE;
    public static void bootstrap(BootstrapContext<ChickenVariant> context) {
        register(context, TEMPERATE, ChickenVariant.ModelType.NORMAL, "chicken_temperate", "chicken_temperate_baby", SpawnPrioritySelectors.fallback(0));
        register(context, WARM, ChickenVariant.ModelType.NORMAL, "chicken_warm", "chicken_warm_baby", net.minecraft.tags.BiomeTags.SPAWNS_WARM_VARIANT_FARM_ANIMALS);
        register(context, COLD, ChickenVariant.ModelType.COLD, "chicken_cold", "chicken_cold_baby", net.minecraft.tags.BiomeTags.SPAWNS_COLD_VARIANT_FARM_ANIMALS);
    }
    private static void register(BootstrapContext<ChickenVariant> context, ResourceKey<ChickenVariant> name, ChickenVariant.ModelType modelType, String textureName,
            String babyTextureName, net.minecraft.tags.TagKey<net.minecraft.world.level.biome.Biome> spawnBiome) {
        net.minecraft.core.HolderSet<net.minecraft.world.level.biome.Biome> biomes = context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(spawnBiome);
        register(context, name, modelType, textureName, babyTextureName, SpawnPrioritySelectors.single(new BiomeCheck(biomes), 1));
    }
    private static void register(BootstrapContext<ChickenVariant> context, ResourceKey<ChickenVariant> name, ChickenVariant.ModelType modelType, String textureName,
            String babyTextureName, SpawnPrioritySelectors selectors) {
        net.minecraft.resources.Identifier texture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/chicken/" + textureName);
        net.minecraft.resources.Identifier babyTexture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/chicken/" + babyTextureName);
        context.register(name, new ChickenVariant(new ModelAndTexture<>(modelType, texture), new ClientAsset.ResourceTexture(babyTexture), selectors));
    }
}
