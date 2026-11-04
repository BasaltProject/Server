// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.flag;

import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.Set;

public class FeatureFlagSet {
    private static final FeatureFlagSet kEmpty = new FeatureFlagSet(Set.of());
    public static final int MAX_CONTAINER_SIZE = 64;
    private final Set<FeatureFlag> flags;

    protected FeatureFlagSet() {
        this(Set.of());
    }

    private FeatureFlagSet(Set<FeatureFlag> flags) {
        this.flags = flags;
    }

    static FeatureFlagSet create(FeatureFlagUniverse universe, Collection<FeatureFlag> flags) {
        return flags.isEmpty() ? kEmpty : new FeatureFlagSet(Set.copyOf(flags));
    }

    public static FeatureFlagSet of() {
        return kEmpty;
    }

    public static FeatureFlagSet of(FeatureFlag flag) {
        return new FeatureFlagSet(Set.of(flag));
    }

    public static FeatureFlagSet of(FeatureFlag flag, FeatureFlag... flags) {
        Set<FeatureFlag> all = new LinkedHashSet<>();
        all.add(flag);
        all.addAll(Set.of(flags));
        return new FeatureFlagSet(Set.copyOf(all));
    }

    public boolean contains(FeatureFlag flag) {
        return flags.contains(flag);
    }

    public boolean isEmpty() {
        return flags.isEmpty();
    }

    public boolean isSubsetOf(FeatureFlagSet set) {
        return set.flags.containsAll(flags);
    }

    public boolean intersects(FeatureFlagSet set) {
        return flags.stream().anyMatch(set.flags::contains);
    }

    public FeatureFlagSet join(FeatureFlagSet other) {
        Set<FeatureFlag> all = new LinkedHashSet<>(flags);
        all.addAll(other.flags);
        return new FeatureFlagSet(Set.copyOf(all));
    }

    public FeatureFlagSet subtract(FeatureFlagSet other) {
        Set<FeatureFlag> rest = new LinkedHashSet<>(flags);
        rest.removeAll(other.flags);
        return new FeatureFlagSet(Set.copyOf(rest));
    }

    public boolean equals(Object o) {
        return o instanceof FeatureFlagSet other && flags.equals(other.flags);
    }

    public int hashCode() {
        return flags.hashCode();
    }
}
