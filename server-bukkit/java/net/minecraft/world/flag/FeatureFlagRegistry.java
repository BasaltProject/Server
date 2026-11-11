// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.flag;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import net.minecraft.resources.Identifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class FeatureFlagRegistry {
    private static final Logger kLog = LoggerFactory.getLogger(FeatureFlagRegistry.class);
    private final FeatureFlagUniverse universe;
    private final Map<Identifier, FeatureFlag> names;
    private final FeatureFlagSet allFlags;
    private FeatureFlagRegistry(FeatureFlagUniverse universe, FeatureFlagSet allFlags, Map<Identifier, FeatureFlag> names) {
        this.universe = universe;
        this.names = names;
        this.allFlags = allFlags;
    }
    public boolean isSubset(FeatureFlagSet set) {
        return set.isSubsetOf(allFlags);
    }
    public FeatureFlagSet allFlags() {
        return allFlags;
    }
    public FeatureFlagSet fromNames(Iterable<Identifier> flagIds) {
        return fromNames(flagIds, id -> kLog.warn("Unknown feature flag: {}", id));
    }
    public FeatureFlagSet subset(FeatureFlag... flags) {
        return FeatureFlagSet.create(universe, Arrays.asList(flags));
    }
    public FeatureFlagSet fromNames(Iterable<Identifier> flagIds, Consumer<Identifier> unknownFlags) {
        Set<FeatureFlag> found = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
        for (Identifier id : flagIds) {
            FeatureFlag flag = names.get(id);
            if (flag == null) {
                unknownFlags.accept(id);
            } else {
                found.add(flag);
            }
        }
        return FeatureFlagSet.create(universe, found);
    }
    public Set<Identifier> toNames(FeatureFlagSet set) {
        Set<Identifier> out = new HashSet<>();
        names.forEach((id, flag) -> {
            if (set.contains(flag)) {
                out.add(id);
            }
        });
        return out;
    }
    public Codec<FeatureFlagSet> codec() {
        return Identifier.CODEC.listOf().comapFlatMap(ids -> {
            Set<Identifier> unknown = new HashSet<>();
            FeatureFlagSet result = fromNames(ids, unknown::add);
            return unknown.isEmpty() ? DataResult.success(result) : DataResult.error(() -> "Unknown feature ids: " + unknown, result);
        }, set -> new ArrayList<>(toNames(set)));
    }
    public static class Builder {
        private final FeatureFlagUniverse universe;
        private int next;
        private final Map<Identifier, FeatureFlag> flags = new LinkedHashMap<>();
        public Builder(String universeId) {
            this.universe = new FeatureFlagUniverse(universeId);
        }
        public FeatureFlag createVanilla(String name) {
            return create(Identifier.withDefaultNamespace(name));
        }
        public FeatureFlag create(Identifier name) {
            if (next >= FeatureFlagSet.MAX_CONTAINER_SIZE) {
                throw new IllegalStateException("Too many feature flags");
            }
            FeatureFlag flag = new FeatureFlag(universe, next++);
            if (flags.put(name, flag) != null) {
                throw new IllegalStateException("Duplicate feature flag " + name);
            }
            return flag;
        }
        public FeatureFlagRegistry build() {
            return new FeatureFlagRegistry(universe, FeatureFlagSet.create(universe, flags.values()), Map.copyOf(flags));
        }
    }
}
