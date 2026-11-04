// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment.trim;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public class TrimPatterns {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.TRIM_PATTERN,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    public static final ResourceKey<TrimPattern> SENTRY = key("sentry");
    public static final ResourceKey<TrimPattern> DUNE = key("dune");
    public static final ResourceKey<TrimPattern> COAST = key("coast");
    public static final ResourceKey<TrimPattern> WILD = key("wild");
    public static final ResourceKey<TrimPattern> WARD = key("ward");
    public static final ResourceKey<TrimPattern> EYE = key("eye");
    public static final ResourceKey<TrimPattern> VEX = key("vex");
    public static final ResourceKey<TrimPattern> TIDE = key("tide");
    public static final ResourceKey<TrimPattern> SNOUT = key("snout");
    public static final ResourceKey<TrimPattern> RIB = key("rib");
    public static final ResourceKey<TrimPattern> SPIRE = key("spire");
    public static final ResourceKey<TrimPattern> WAYFINDER = key("wayfinder");
    public static final ResourceKey<TrimPattern> SHAPER = key("shaper");
    public static final ResourceKey<TrimPattern> SILENCE = key("silence");
    public static final ResourceKey<TrimPattern> RAISER = key("raiser");
    public static final ResourceKey<TrimPattern> HOST = key("host");
    public static final ResourceKey<TrimPattern> FLOW = key("flow");
    public static final ResourceKey<TrimPattern> BOLT = key("bolt");
    public static void bootstrap(BootstrapContext<TrimPattern> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.trim.TrimPatterns#bootstrap");
    }
    public static void register(BootstrapContext<TrimPattern> context, ResourceKey<TrimPattern> registryKey) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.trim.TrimPatterns#register");
    }
    public static Identifier defaultAssetId(ResourceKey<TrimPattern> registryKey) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.equipment.trim.TrimPatterns#defaultAssetId");
    }
}
