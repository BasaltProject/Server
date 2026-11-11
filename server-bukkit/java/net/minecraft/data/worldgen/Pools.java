// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class Pools {
    public static final ResourceKey<StructureTemplatePool> EMPTY = k("empty");
    public static ResourceKey<StructureTemplatePool> createKey(Identifier location) {
        return ResourceKey.create((ResourceKey) Registries.TEMPLATE_POOL, location);
    }
    public static ResourceKey<StructureTemplatePool> createKey(String name) {
        return createKey(Identifier.withDefaultNamespace(name));
    }
    public static ResourceKey<StructureTemplatePool> parseKey(String name) {
        return createKey(Identifier.parse(name));
    }
    public static void register(BootstrapContext<StructureTemplatePool> context, String name, StructureTemplatePool pool) {
    }
    public static void bootstrap(BootstrapContext<StructureTemplatePool> context) {
    }

    private static ResourceKey<StructureTemplatePool> k(String n) {
        return createKey(n);
    }
}
