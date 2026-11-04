// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.pig;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.ModelAndTexture;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class PigVariants {
    public static final ResourceKey<PigVariant> TEMPERATE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PIG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("temperate"));
    public static final ResourceKey<PigVariant> WARM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PIG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("warm"));
    public static final ResourceKey<PigVariant> COLD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PIG_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cold"));
    public static final ResourceKey<PigVariant> DEFAULT = TEMPERATE;
    public static void bootstrap(BootstrapContext<PigVariant> context) {
        register(context, TEMPERATE, PigVariant.ModelType.NORMAL, "pig_temperate", "pig_temperate_baby", SpawnPrioritySelectors.fallback(0));
        register(context, WARM, PigVariant.ModelType.NORMAL, "pig_warm", "pig_warm_baby", net.minecraft.tags.BiomeTags.SPAWNS_WARM_VARIANT_FARM_ANIMALS);
        register(context, COLD, PigVariant.ModelType.COLD, "pig_cold", "pig_cold_baby", net.minecraft.tags.BiomeTags.SPAWNS_COLD_VARIANT_FARM_ANIMALS);
    }
    private static void register(BootstrapContext<PigVariant> context, ResourceKey<PigVariant> name, PigVariant.ModelType modelType, String textureName,
            String babyTextureName, net.minecraft.tags.TagKey<net.minecraft.world.level.biome.Biome> spawnBiome) {
        net.minecraft.core.HolderSet<net.minecraft.world.level.biome.Biome> biomes = context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(spawnBiome);
        register(context, name, modelType, textureName, babyTextureName, SpawnPrioritySelectors.single(new BiomeCheck(biomes), 1));
    }
    private static void register(BootstrapContext<PigVariant> context, ResourceKey<PigVariant> name, PigVariant.ModelType modelType, String textureName,
            String babyTextureName, SpawnPrioritySelectors selectors) {
        net.minecraft.resources.Identifier texture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/pig/" + textureName);
        net.minecraft.resources.Identifier babyTexture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/pig/" + babyTextureName);
        context.register(name, new PigVariant(new ModelAndTexture<>(modelType, texture), new ClientAsset.ResourceTexture(babyTexture), selectors));
    }
}
