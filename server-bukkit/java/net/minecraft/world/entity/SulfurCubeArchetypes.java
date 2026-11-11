// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class SulfurCubeArchetypes {
    public static final ResourceKey<SulfurCubeArchetype> REGULAR = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("regular"));
    public static final ResourceKey<SulfurCubeArchetype> BOUNCY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("bouncy"));
    public static final ResourceKey<SulfurCubeArchetype> SLOW_BOUNCY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("slow_bouncy"));
    public static final ResourceKey<SulfurCubeArchetype> SLOW_FLAT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("slow_flat"));
    public static final ResourceKey<SulfurCubeArchetype> FAST_FLAT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("fast_flat"));
    public static final ResourceKey<SulfurCubeArchetype> LIGHT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("light"));
    public static final ResourceKey<SulfurCubeArchetype> FAST_SLIDING = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("fast_sliding"));
    public static final ResourceKey<SulfurCubeArchetype> SLOW_SLIDING = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("slow_sliding"));
    public static final ResourceKey<SulfurCubeArchetype> HIGH_RESISTANCE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("high_resistance"));
    public static final ResourceKey<SulfurCubeArchetype> STICKY = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("sticky"));
    public static final ResourceKey<SulfurCubeArchetype> EXPLOSIVE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("explosive"));
    public static final ResourceKey<SulfurCubeArchetype> HOT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.SULFUR_CUBE_ARCHETYPE, net.minecraft.resources.Identifier.withDefaultNamespace("hot"));
    public static void bootstrap(BootstrapContext<SulfurCubeArchetype> context) {
        // the archetypes come from the extracted datapack, so there is nothing for a bootstrap to add
    }
}
