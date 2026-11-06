// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
public class Climate {
    protected Climate() {
    }

    public record Sampler(Object temperature, Object humidity, Object continentalness, Object erosion, Object depth, Object weirdness) {
        public static Sampler forWorld(String world) {
            WorldSource source = new WorldSource(world);
            return new Sampler(source, source, source, source, source, source);
        }
        public TargetPoint sample(int quartX, int quartY, int quartZ) {
            if (!(temperature instanceof WorldSource source)) {
                throw new UnsupportedOperationException("Only the sampler of a loaded world can be evaluated");
            }
            long[] point = org.basaltmc.bukkit.LeftoverNatives.climate(source.world(), quartX, quartY, quartZ);
            if (point == null) {
                throw new IllegalStateException("World " + source.world() + " has no multi-noise biome source");
            }
            return new TargetPoint(point[0], point[1], point[2], point[3], point[4], point[5]);
        }
    }
    private record WorldSource(String world) {
    }
    protected static final int PARAMETER_COUNT = 7;
    public static TargetPoint target(float temperature, float humidity, float continentalness, float erosion, float depth, float weirdness) {
        return new TargetPoint(quantizeCoord(temperature), quantizeCoord(humidity), quantizeCoord(continentalness),
            quantizeCoord(erosion), quantizeCoord(depth), quantizeCoord(weirdness));
    }
    public static ParameterPoint parameters(float temperature, float humidity, float continentalness, float erosion, float depth, float weirdness, float offset) {
        return new ParameterPoint(Parameter.point(temperature), Parameter.point(humidity), Parameter.point(continentalness),
            Parameter.point(erosion), Parameter.point(depth), Parameter.point(weirdness), quantizeCoord(offset));
    }
    public static ParameterPoint parameters(Parameter temperature, Parameter humidity, Parameter continentalness, Parameter erosion, Parameter depth, Parameter weirdness, float offset) {
        return new ParameterPoint(temperature, humidity, continentalness, erosion, depth, weirdness, quantizeCoord(offset));
    }
    public static long quantizeCoord(float coord) {
        return (long) (coord * 10000.0F);
    }
    public static float unquantizeCoord(long coord) {
        return (float) coord / 10000.0F;
    }
    protected static final class RTree<T> {
        private final Node<T> root;
        private RTree(Node<T> root) {
            this.root = root;
        }
        public static <T> RTree<T> create(List<Pair<ParameterPoint, T>> values) {
            return create(values, 6);
        }
        public static <T> RTree<T> create(List<Pair<ParameterPoint, T>> values, int childrenPerNode) {
            if (values.isEmpty()) {
                throw new IllegalArgumentException("Need at least one value to build the search tree.");
            }
            List<Leaf<T>> leaves = new java.util.ArrayList<>(values.size());
            for (Pair<ParameterPoint, T> entry : values) {
                leaves.add(new Leaf<>(entry.getFirst(), entry.getSecond()));
            }
            return new RTree<>(new SubTree<>(leaves));
        }
        public T search(TargetPoint target, DistanceMetric<T> distanceMetric) {
            return root.search(target.toParameterArray(), null, distanceMetric).value;
        }
        static abstract class Node<T> {
            protected final Parameter[] parameterSpace;
            protected Node(List<Parameter> parameterSpace) {
                this.parameterSpace = parameterSpace.toArray(new Parameter[0]);
            }
            protected abstract Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric);
            protected long distance(long[] target) {
                long total = 0L;
                for (int i = 0; i < PARAMETER_COUNT; i++) {
                    long gap = parameterSpace[i].distance(target[i]);
                    total += gap * gap;
                }
                return total;
            }
            public String toString() {
                return java.util.Arrays.toString(parameterSpace);
            }
        }
        static final class Leaf<T> extends Node<T> {
            final ParameterPoint parameterPoint;
            final T value;
            private Leaf(ParameterPoint parameterPoint, T value) {
                super(parameterPoint.parameterSpace());
                this.parameterPoint = parameterPoint;
                this.value = value;
            }
            protected Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric) {
                if (candidate == null) {
                    return this;
                }
                return distanceMetric.distance(this, target) < distanceMetric.distance(candidate, target) ? this : candidate;
            }
        }
        static final class SubTree<T> extends Node<T> {
            private final Node<T>[] children;
            @SuppressWarnings("unchecked")
            public SubTree(List<? extends Node<T>> children) {
                this(spaceOf(children), children);
            }
            @SuppressWarnings("unchecked")
            public SubTree(List<Parameter> parameterSpace, List<? extends Node<T>> children) {
                super(parameterSpace);
                this.children = children.toArray(new Node[0]);
            }
            private static <T> List<Parameter> spaceOf(List<? extends Node<T>> children) {
                Parameter[] merged = new Parameter[PARAMETER_COUNT];
                for (Node<T> child : children) {
                    for (int i = 0; i < PARAMETER_COUNT; i++) {
                        merged[i] = merged[i] == null ? child.parameterSpace[i] : merged[i].span(child.parameterSpace[i]);
                    }
                }
                return java.util.Arrays.asList(merged);
            }
            protected Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric) {
                long best = candidate == null ? Long.MAX_VALUE : distanceMetric.distance(candidate, target);
                Leaf<T> found = candidate;
                for (Node<T> child : children) {
                    if (distanceMetric.distance(child, target) >= best) {
                        continue;
                    }
                    Leaf<T> inner = child.search(target, found, distanceMetric);
                    long innerDistance = inner == null ? Long.MAX_VALUE : distanceMetric.distance(inner, target);
                    if (innerDistance < best) {
                        best = innerDistance;
                        found = inner;
                    }
                }
                return found;
            }
        }
    }
    public static class ParameterList<T> {
        private final List<Pair<ParameterPoint, T>> entries;
        public static <T> Codec<ParameterList<T>> codec(MapCodec<T> valueCodec) {
            Codec<Pair<ParameterPoint, T>> entry = com.mojang.serialization.codecs.RecordCodecBuilder.<Pair<ParameterPoint, T>>create(i -> i.group(
                ParameterPoint.CODEC.fieldOf("parameters").forGetter(Pair::getFirst),
                valueCodec.forGetter(Pair::getSecond)).apply(i, Pair::of));
            return entry.listOf().flatXmap(list -> list.isEmpty() ? com.mojang.serialization.DataResult.error(() -> "List must have contents")
                : com.mojang.serialization.DataResult.success(new ParameterList<T>(list)), list -> com.mojang.serialization.DataResult.success(list.values()));
        }
        public ParameterList(List<Pair<ParameterPoint, T>> values) {
            this.entries = List.copyOf(values);
        }
        private ParameterList(List<Pair<ParameterPoint, T>> values, int childrenPerNode) {
            this.entries = List.copyOf(values);
        }
        public ParameterList<T> rebuildWithChildrenPerNode(int childrenPerNode) {
            return new ParameterList<>(entries);
        }
        public List<Pair<ParameterPoint, T>> values() {
            return entries;
        }
        public T findValue(TargetPoint target) {
            return findValueBruteForce(target);
        }
        public T findValueBruteForce(TargetPoint target) {
            long best = Long.MAX_VALUE;
            T found = null;
            for (Pair<ParameterPoint, T> entry : entries) {
                long fitness = entry.getFirst().fitness(target);
                if (fitness < best) {
                    best = fitness;
                    found = entry.getSecond();
                }
            }
            return found;
        }
        public T findValueIndex(TargetPoint target) {
            return findValueBruteForce(target);
        }
        protected T findValueIndex(TargetPoint target, DistanceMetric<T> distanceMetric) {
            return RTree.create(entries).search(target, distanceMetric);
        }
    }
    public static record TargetPoint(long temperature, long humidity, long continentalness, long erosion, long depth, long weirdness) {
        long[] toParameterArray() {
            return new long[] {temperature, humidity, continentalness, erosion, depth, weirdness, 0L};
        }
    }
    public static record ParameterPoint(Parameter temperature, Parameter humidity, Parameter continentalness, Parameter erosion, Parameter depth, Parameter weirdness, long offset) {
        public static final Codec<ParameterPoint> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Climate.ParameterPoint.class, "CODEC");
        private static long sq(long value) {
            return value * value;
        }
        public long fitness(TargetPoint target) {
            return sq(temperature.distance(target.temperature())) + sq(humidity.distance(target.humidity()))
                + sq(continentalness.distance(target.continentalness())) + sq(erosion.distance(target.erosion()))
                + sq(depth.distance(target.depth())) + sq(weirdness.distance(target.weirdness())) + sq(offset);
        }
        List<Parameter> parameterSpace() {
            return List.of(temperature, humidity, continentalness, erosion, depth, weirdness, new Parameter(offset, offset));
        }
    }
    public static record Parameter(long min, long max) {
        public static final Codec<Parameter> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.Climate.Parameter.class, "CODEC");
        public static Parameter point(float min) {
            return span(min, min);
        }
        public static Parameter span(float min, float max) {
            if (min > max) {
                throw new IllegalArgumentException("min > max: " + min + " " + max);
            }
            return new Parameter(quantizeCoord(min), quantizeCoord(max));
        }
        public static Parameter span(Parameter min, Parameter max) {
            if (min.min() > max.max()) {
                throw new IllegalArgumentException("min > max: " + min + " , " + max);
            }
            return new Parameter(min.min(), max.max());
        }
        public long distance(long target) {
            long above = target - max;
            long below = min - target;
            return above > 0L ? above : Math.max(below, 0L);
        }
        public Parameter span(Parameter other) {
            return new Parameter(Math.min(min, other.min()), Math.max(max, other.max()));
        }
    }
    interface DistanceMetric<T> {
        long distance(RTree.Node<T> node, long[] target);
    }
}
