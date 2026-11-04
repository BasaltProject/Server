// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.wolf;
import net.minecraft.core.ClientAsset;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class WolfVariants {
    public static final ResourceKey<WolfVariant> PALE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("pale"));
    public static final ResourceKey<WolfVariant> SPOTTED = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("spotted"));
    public static final ResourceKey<WolfVariant> SNOWY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("snowy"));
    public static final ResourceKey<WolfVariant> BLACK = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("black"));
    public static final ResourceKey<WolfVariant> ASHEN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("ashen"));
    public static final ResourceKey<WolfVariant> RUSTY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("rusty"));
    public static final ResourceKey<WolfVariant> WOODS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("woods"));
    public static final ResourceKey<WolfVariant> CHESTNUT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("chestnut"));
    public static final ResourceKey<WolfVariant> STRIPED = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.WOLF_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("striped"));
    public static final ResourceKey<WolfVariant> DEFAULT = PALE;
    public static void bootstrap(BootstrapContext<WolfVariant> context) {
        register(context, PALE, "wolf", SpawnPrioritySelectors.fallback(0));
        register(context, SPOTTED, "wolf_spotted", net.minecraft.tags.BiomeTags.IS_SAVANNA);
        register(context, SNOWY, "wolf_snowy", net.minecraft.world.level.biome.Biomes.GROVE);
        register(context, BLACK, "wolf_black", net.minecraft.world.level.biome.Biomes.OLD_GROWTH_PINE_TAIGA);
        register(context, ASHEN, "wolf_ashen", net.minecraft.world.level.biome.Biomes.SNOWY_TAIGA);
        register(context, RUSTY, "wolf_rusty", net.minecraft.tags.BiomeTags.IS_JUNGLE);
        register(context, WOODS, "wolf_woods", net.minecraft.world.level.biome.Biomes.FOREST);
        register(context, CHESTNUT, "wolf_chestnut", net.minecraft.world.level.biome.Biomes.OLD_GROWTH_SPRUCE_TAIGA);
        register(context, STRIPED, "wolf_striped", net.minecraft.tags.BiomeTags.IS_BADLANDS);
    }
    private static void register(BootstrapContext<WolfVariant> context, ResourceKey<WolfVariant> name, String fileName, ResourceKey<net.minecraft.world.level.biome.Biome> spawnBiome) {
        register(context, name, fileName, highPrioBiome(net.minecraft.core.HolderSet.direct(context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(spawnBiome))));
    }
    private static void register(BootstrapContext<WolfVariant> context, ResourceKey<WolfVariant> name, String fileName, net.minecraft.tags.TagKey<net.minecraft.world.level.biome.Biome> spawnBiome) {
        register(context, name, fileName, highPrioBiome(context.lookup(net.minecraft.core.registries.Registries.BIOME).getOrThrow(spawnBiome)));
    }
    private static SpawnPrioritySelectors highPrioBiome(net.minecraft.core.HolderSet<net.minecraft.world.level.biome.Biome> biomes) {
        return SpawnPrioritySelectors.single(new BiomeCheck(biomes), 1);
    }
    private static ClientAsset.ResourceTexture texture(String name) {
        return new ClientAsset.ResourceTexture(net.minecraft.resources.Identifier.withDefaultNamespace("entity/wolf/" + name));
    }
    private static void register(BootstrapContext<WolfVariant> context, ResourceKey<WolfVariant> name, String fileName, SpawnPrioritySelectors selectors) {
        context.register(name, new WolfVariant(new WolfVariant.AssetInfo(texture(fileName), texture(fileName + "_tame"), texture(fileName + "_angry")),
            new WolfVariant.AssetInfo(texture(fileName + "_baby"), texture(fileName + "_tame_baby"), texture(fileName + "_angry_baby")), selectors));
    }
}
