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
            public TargetPoint sample(int quartX, int quartY, int quartZ) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.Sampler#sample");
        }
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
        private RTree(Node<T> root) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree#<init>");
        }
        public static <T> RTree<T> create(List<Pair<ParameterPoint, T>> values) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree#create");
        }
        public static <T> RTree<T> create(List<Pair<ParameterPoint, T>> values, int childrenPerNode) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree#create");
        }
        public T search(TargetPoint target, DistanceMetric<T> distanceMetric) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree#search");
        }
        static abstract class Node<T> {
            protected final Parameter[] parameterSpace = null;
            protected Node(List<Parameter> parameterSpace) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.Node#<init>");
            }
            protected abstract Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric);
            protected long distance(long[] target) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.Node#distance");
            }
            public String toString() {
                return super.toString();
            }
        }
        static final class Leaf<T> extends Node<T> {
            private Leaf(ParameterPoint parameterPoint, T value) {
                super((java.util.List) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.Leaf#<init>");
            }
            protected Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.Leaf#search");
            }
        }
        static final class SubTree<T> extends Node<T> {
            public SubTree(List<? extends Node<T>> children) {
                super((java.util.List) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.SubTree#<init>");
            }
            public SubTree(List<Parameter> parameterSpace, List<? extends Node<T>> children) {
                super((java.util.List) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.SubTree#<init>");
            }
            protected Leaf<T> search(long[] target, Leaf<T> candidate, DistanceMetric<T> distanceMetric) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.RTree.SubTree#search");
            }
        }
    }
    public static class ParameterList<T> {
        private final List<Pair<ParameterPoint, T>> entries;
        public static <T> Codec<ParameterList<T>> codec(MapCodec<T> valueCodec) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.ParameterList#codec");
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
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.Climate.ParameterList#findValueIndex");
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
