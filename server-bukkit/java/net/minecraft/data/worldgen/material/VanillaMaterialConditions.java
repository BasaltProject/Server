// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.material;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.material.condition.MaterialCondition;
public class VanillaMaterialConditions {
    public static final ResourceKey<MaterialCondition> ON_FLOOR = k("on_floor");
    public static final ResourceKey<MaterialCondition> UNDER_FLOOR = k("under_floor");
    public static final ResourceKey<MaterialCondition> DEEP_UNDER_FLOOR = k("deep_under_floor");
    public static final ResourceKey<MaterialCondition> VERY_DEEP_UNDER_FLOOR = k("very_deep_under_floor");
    public static final ResourceKey<MaterialCondition> ON_CEILING = k("on_ceiling");
    public static final ResourceKey<MaterialCondition> UNDER_CEILING = k("under_ceiling");
    public static final ResourceKey<MaterialCondition> NOT_UNDERWATER = k("not_underwater");
    public static final ResourceKey<MaterialCondition> NOT_UNDER_DEEP_WATER = k("not_under_deep_water");
    public static void bootstrap(BootstrapContext<MaterialCondition> context) {
    }

    private static ResourceKey<MaterialCondition> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.MATERIAL_CONDITION, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
