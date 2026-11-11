// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.material;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
public class NetherMaterialRules {
    public static final ResourceKey<MaterialRule> NETHER = k("nether");
    public static void bootstrap(BootstrapContext<MaterialRule> context) {
    }

    private static ResourceKey<MaterialRule> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.MATERIAL_RULE, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
