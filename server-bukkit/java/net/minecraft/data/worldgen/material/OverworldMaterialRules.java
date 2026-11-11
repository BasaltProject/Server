// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.material;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
public class OverworldMaterialRules {
    public static final ResourceKey<MaterialRule> OVERWORLD = k("overworld");
    public static final ResourceKey<MaterialRule> OVERWORLD_CAVES = k("overworld_caves");
    public static final ResourceKey<MaterialRule> OVERWORLD_FLOATING_ISLANDS = k("overworld_floating_islands");
    public static void bootstrap(BootstrapContext<MaterialRule> context) {
    }

    private static ResourceKey<MaterialRule> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.MATERIAL_RULE, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
