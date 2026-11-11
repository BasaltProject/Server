// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;
import net.minecraft.world.entity.variant.BiomeCheck;
import net.minecraft.world.entity.variant.SpawnPrioritySelectors;
import net.minecraft.world.entity.variant.ModelAndTexture;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.tags.TagKey;
import net.minecraft.tags.BiomeTags;
import net.minecraft.resources.Identifier;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class ZombieNautilusVariants {
    private static void put(BootstrapContext<ZombieNautilusVariant> context, ResourceKey<ZombieNautilusVariant> key, ZombieNautilusVariant.ModelType model, String texture, SpawnPrioritySelectors selectors) {
        context.register(key, new ZombieNautilusVariant(new ModelAndTexture<>(model, Identifier.withDefaultNamespace("entity/nautilus/" + texture)), selectors));
    }

    public static final ResourceKey<ZombieNautilusVariant> TEMPERATE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.ZOMBIE_NAUTILUS_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("temperate"));
    public static final ResourceKey<ZombieNautilusVariant> WARM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.ZOMBIE_NAUTILUS_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("warm"));
    public static final ResourceKey<ZombieNautilusVariant> DEFAULT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.ZOMBIE_NAUTILUS_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("default"));
    public static void bootstrap(BootstrapContext<ZombieNautilusVariant> context) {
        HolderSet<Biome> coral = context.lookup(Registries.BIOME).getOrThrow(BiomeTags.SPAWNS_CORAL_VARIANT_ZOMBIE_NAUTILUS);
        put(context, TEMPERATE, ZombieNautilusVariant.ModelType.NORMAL, "zombie_nautilus", SpawnPrioritySelectors.fallback(0));
        put(context, WARM, ZombieNautilusVariant.ModelType.WARM, "zombie_nautilus_coral", SpawnPrioritySelectors.single(new BiomeCheck(coral), 1));
    }
}
