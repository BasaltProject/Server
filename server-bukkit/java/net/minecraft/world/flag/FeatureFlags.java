// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.flag;
import com.mojang.serialization.Codec;
import java.util.Set;
import java.util.stream.Collectors;
import net.minecraft.resources.Identifier;
public class FeatureFlags {
    private static final FeatureFlagRegistry.Builder kBuilder = new FeatureFlagRegistry.Builder("main");
    public static final FeatureFlag VANILLA = kBuilder.createVanilla("vanilla");
    public static final FeatureFlag TRADE_REBALANCE = kBuilder.createVanilla("trade_rebalance");
    public static final FeatureFlag REDSTONE_EXPERIMENTS = kBuilder.createVanilla("redstone_experiments");
    public static final FeatureFlag MINECART_IMPROVEMENTS = kBuilder.createVanilla("minecart_improvements");
    public static final FeatureFlagRegistry REGISTRY = kBuilder.build();
    public static final Codec<FeatureFlagSet> CODEC = REGISTRY.codec();
    public static final FeatureFlagSet VANILLA_SET = FeatureFlagSet.of(VANILLA);
    public static final FeatureFlagSet DEFAULT_FLAGS = VANILLA_SET;
    public static String printMissingFlags(FeatureFlagSet allowedFlags, FeatureFlagSet requestedFlags) {
        return printMissingFlags(REGISTRY, allowedFlags, requestedFlags);
    }
    public static String printMissingFlags(FeatureFlagRegistry registry, FeatureFlagSet allowedFlags, FeatureFlagSet requestedFlags) {
        Set<Identifier> allowed = registry.toNames(allowedFlags);
        return registry.toNames(requestedFlags).stream().filter(id -> !allowed.contains(id)).map(Identifier::toString).collect(Collectors.joining(", "));
    }
    public static boolean isExperimental(FeatureFlagSet features) {
        return !features.isSubsetOf(VANILLA_SET);
    }
}
