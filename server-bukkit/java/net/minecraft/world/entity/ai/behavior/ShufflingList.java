// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Random;
import java.util.stream.Stream;

public class ShufflingList<U> implements Iterable<U> {
    protected final List<WeightedEntry<U>> entries;
    private final Random random = new Random();

    public ShufflingList() {
        entries = new ArrayList<>();
    }

    private ShufflingList(List<WeightedEntry<U>> entries) {
        this.entries = new ArrayList<>(entries);
    }

    public static <U> Codec<ShufflingList<U>> codec(Codec<U> elementCodec) {
        return WeightedEntry.codec(elementCodec).listOf().xmap(ShufflingList::new, list -> list.entries);
    }

    public ShufflingList<U> add(U data, int weight) {
        entries.add(new WeightedEntry<>(data, weight));
        return this;
    }

    public ShufflingList<U> shuffle() {
        entries.forEach(entry -> entry.setRandom(random.nextFloat()));
        entries.sort(java.util.Comparator.comparingDouble(WeightedEntry::getRandWeight));
        return this;
    }

    public Stream<U> stream() {
        return entries.stream().map(WeightedEntry::getData);
    }

    @Override
    public Iterator<U> iterator() {
        return entries.stream().map(WeightedEntry::getData).iterator();
    }

    @Override
    public String toString() {
        return "ShufflingList[" + entries + "]";
    }

    public static class WeightedEntry<T> {
        private final T data;
        private final int weight;
        private double randWeight;

        private WeightedEntry(T data, int weight) {
            this.weight = weight;
            this.data = data;
        }

        private double getRandWeight() {
            return randWeight;
        }

        private void setRandom(float random) {
            randWeight = -Math.pow(random, 1.0F / weight);
        }

        public T getData() {
            return data;
        }

        public int getWeight() {
            return weight;
        }

        @Override
        public String toString() {
            return weight + ":" + data;
        }

        public static <E> Codec<WeightedEntry<E>> codec(Codec<E> elementCodec) {
            return RecordCodecBuilder.create(instance -> instance.group(elementCodec.fieldOf("data").forGetter(WeightedEntry::getData),
                Codec.INT.fieldOf("weight").forGetter(WeightedEntry::getWeight)).apply(instance, WeightedEntry::new));
        }
    }
}
