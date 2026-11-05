// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment.trim;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public class TrimMaterials {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.TRIM_MATERIAL,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    public static final ResourceKey<TrimMaterial> QUARTZ = key("quartz");
    public static final ResourceKey<TrimMaterial> IRON = key("iron");
    public static final ResourceKey<TrimMaterial> NETHERITE = key("netherite");
    public static final ResourceKey<TrimMaterial> REDSTONE = key("redstone");
    public static final ResourceKey<TrimMaterial> COPPER = key("copper");
    public static final ResourceKey<TrimMaterial> GOLD = key("gold");
    public static final ResourceKey<TrimMaterial> EMERALD = key("emerald");
    public static final ResourceKey<TrimMaterial> DIAMOND = key("diamond");
    public static final ResourceKey<TrimMaterial> LAPIS = key("lapis");
    public static final ResourceKey<TrimMaterial> AMETHYST = key("amethyst");
    public static final ResourceKey<TrimMaterial> RESIN = key("resin");
    public static void bootstrap(BootstrapContext<TrimMaterial> context) {
        org.basaltmc.bukkit.facade.Bootstraps.replay(context, net.minecraft.core.registries.Registries.TRIM_MATERIAL, TrimMaterials.class);
    }
    public static enum Palette {
        QUARTZ, IRON, IRON_DARKER, NETHERITE, NETHERITE_DARKER, REDSTONE, COPPER, COPPER_DARKER, GOLD, GOLD_DARKER, EMERALD, DIAMOND, DIAMOND_DARKER, LAPIS, AMETHYST, RESIN;
        public String suffix() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public Identifier id() {
            return Identifier.withDefaultNamespace("trim/" + suffix());
        }
    }
}
