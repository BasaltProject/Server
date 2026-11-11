// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.material;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.material.rule.MaterialRule;
public class VanillaMaterialRules {
    public static final ResourceKey<MaterialRule> BEDROCK_FLOOR = k("bedrock_floor");
    public static final ResourceKey<MaterialRule> BEDROCK_ROOF = k("bedrock_roof");
    public static void bootstrap(BootstrapContext<MaterialRule> context) {
    }
    public static MaterialRule air() {
        return null;
    }

    private static ResourceKey<MaterialRule> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.MATERIAL_RULE, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
