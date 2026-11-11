// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import it.unimi.dsi.fastutil.objects.Reference2IntOpenHashMap;
import net.minecraft.util.Graph;
import net.minecraft.util.Util;
import java.util.Map;
import java.util.Set;
import java.util.ArrayList;
import net.minecraft.core.Holder;
import java.util.List;
import java.util.function.Function;
import java.util.function.ToIntFunction;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class FeatureSorter {
    private record Slot(int index, int step, PlacedFeature feature) {
    }
    public static <T> List<StepFeatureData> buildFeaturesPerStep(List<T> featureSources, Function<T, List<HolderSet<PlacedFeature>>> featureGetter, boolean tryReducingError) {
        Reference2IntOpenHashMap<PlacedFeature> indexOf = new Reference2IntOpenHashMap<>();
        int[] counter = {0};
        Comparator<Slot> order = Comparator.comparingInt(Slot::step).thenComparingInt(Slot::index);
        Map<Slot, Set<Slot>> edges = new TreeMap<>(order);
        int steps = 0;
        for (T source : featureSources) {
            List<Slot> chain = new ArrayList<>();
            List<HolderSet<PlacedFeature>> perStep = featureGetter.apply(source);
            steps = Math.max(steps, perStep.size());
            for (int step = 0; step < perStep.size(); step++) {
                for (Holder<PlacedFeature> holder : perStep.get(step)) {
                    PlacedFeature placed = holder.value();
                    int index = indexOf.computeIfAbsent(placed, key -> counter[0]++);
                    chain.add(new Slot(index, step, placed));
                }
            }
            for (int at = 0; at < chain.size(); at++) {
                Set<Slot> next = edges.computeIfAbsent(chain.get(at), key -> new TreeSet<>(order));
                if (at < chain.size() - 1) {
                    next.add(chain.get(at + 1));
                }
            }
        }
        Set<Slot> seen = new TreeSet<>(order);
        Set<Slot> inProgress = new TreeSet<>(order);
        List<Slot> sorted = new ArrayList<>();
        for (Slot slot : edges.keySet()) {
            if (!inProgress.isEmpty()) {
                throw new IllegalStateException("You somehow broke the universe; DFS bork (iteration finished with non-empty in-progress vertex set");
            }
            if (seen.contains(slot) || !Graph.depthFirstSearch(edges, seen, inProgress, sorted::add, slot)) {
                continue;
            }
            if (!tryReducingError) {
                throw new IllegalStateException("Feature order cycle found");
            }
            List<T> culprits = new ArrayList<>(featureSources);
            int before;
            do {
                before = culprits.size();
                ListIterator<T> each = culprits.listIterator();
                while (each.hasNext()) {
                    T candidate = each.next();
                    each.remove();
                    try {
                        buildFeaturesPerStep(culprits, featureGetter, false);
                    } catch (IllegalStateException cyclic) {
                        continue;
                    }
                    each.add(candidate);
                }
            } while (before != culprits.size());
            throw new IllegalStateException("Feature order cycle found, involved sources: " + culprits);
        }
        Collections.reverse(sorted);
        List<StepFeatureData> out = new ArrayList<>();
        for (int step = 0; step < steps; step++) {
            int wanted = step;
            out.add(new StepFeatureData(sorted.stream().filter(slot -> slot.step() == wanted).map(Slot::feature).collect(java.util.stream.Collectors.toList())));
        }
        return List.copyOf(out);
    }
    public static record StepFeatureData(List<PlacedFeature> features, ToIntFunction<PlacedFeature> indexMapping) {
        private StepFeatureData(List<PlacedFeature> features) {
            this(features, Util.createIndexIdentityLookup(features));
        }
    }
}
