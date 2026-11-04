// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.cow;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.ModelAndTexture;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class CowVariants {
    public static final ResourceKey<CowVariant> TEMPERATE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.COW_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("temperate"));
    public static final ResourceKey<CowVariant> WARM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.COW_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("warm"));
    public static final ResourceKey<CowVariant> COLD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.COW_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cold"));
    public static final ResourceKey<CowVariant> DEFAULT = TEMPERATE;
    public static void bootstrap(BootstrapContext<CowVariant> context) {
        register(context, TEMPERATE, CowVariant.ModelType.NORMAL, "cow_temperate", "cow_temperate_baby", SpawnPrioritySelectors.fallback(0));
        register(context, WARM, CowVariant.ModelType.WARM, "cow_warm", "cow_warm_baby", net.minecraft.tags.BiomeTags.SPAWNS_WARM_VARIANT_FARM_ANIMALS);
        register(context, COLD, CowVariant.ModelType.COLD, "cow_cold", "cow_cold_baby", net.minecraft.tags.BiomeTags.SPAWNS_COLD_VARIANT_FARM_ANIMALS);
    }
    private static void register(BootstrapContext<CowVariant> context, ResourceKey<CowVariant> name, CowVariant.ModelType modelType, String textureName,
            String babyTextureName, net.minecraft.tags.TagKey<net.minecraft.world.level.biome.Biome> spawnBiome) {
        net.minecraft.core.HolderSet<net.minecraft.world.level.biome.Biome> biomes = context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(spawnBiome);
        register(context, name, modelType, textureName, babyTextureName, SpawnPrioritySelectors.single(new BiomeCheck(biomes), 1));
    }
    private static void register(BootstrapContext<CowVariant> context, ResourceKey<CowVariant> name, CowVariant.ModelType modelType, String textureName,
            String babyTextureName, SpawnPrioritySelectors selectors) {
        net.minecraft.resources.Identifier texture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/cow/" + textureName);
        net.minecraft.resources.Identifier babyTexture = net.minecraft.resources.Identifier.withDefaultNamespace("entity/cow/" + babyTextureName);
        context.register(name, new CowVariant(new ModelAndTexture<>(modelType, texture), new ClientAsset.ResourceTexture(babyTexture), selectors));
    }
}
