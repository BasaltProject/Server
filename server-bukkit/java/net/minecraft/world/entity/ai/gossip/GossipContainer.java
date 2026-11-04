// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.gossip;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import it.unimi.dsi.fastutil.objects.ObjectIterator;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.function.DoublePredicate;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.util.RandomSource;

public class GossipContainer {
    public static final Codec<GossipContainer> CODEC = Codec.lazyInitialized(() -> GossipEntry.CODEC.listOf()
        .xmap(GossipContainer::new, container -> container.unpack().toList()));
    public static final int DISCARD_THRESHOLD = 2;

    private final Map<UUID, EntityGossips> gossips = new HashMap<>();

    public GossipContainer() {
    }

    private GossipContainer(List<GossipEntry> entries) {
        entries.forEach(entry -> getOrCreate(entry.target).entries.put(entry.type, entry.value));
    }

    public Map<UUID, Object2IntMap<GossipType>> getGossipEntries() {
        Map<UUID, Object2IntMap<GossipType>> result = new HashMap<>();
        gossips.forEach((uuid, entityGossips) -> result.put(uuid, entityGossips.entries));
        return result;
    }

    public void decay() {
        Iterator<EntityGossips> iterator = gossips.values().iterator();
        while (iterator.hasNext()) {
            EntityGossips entityGossips = iterator.next();
            entityGossips.decay();
            if (entityGossips.isEmpty()) {
                iterator.remove();
            }
        }
    }

    private Stream<GossipEntry> unpack() {
        return gossips.entrySet().stream().flatMap(entry -> entry.getValue().unpack(entry.getKey()));
    }

    private Collection<GossipEntry> selectGossipsForTransfer(RandomSource random, int maxCount) {
        List<GossipEntry> entries = unpack().toList();
        if (entries.isEmpty()) {
            return Collections.emptyList();
        }
        int[] ranges = new int[entries.size()];
        int rangesEnd = 0;
        for (int i = 0; i < entries.size(); i++) {
            rangesEnd += Math.abs(entries.get(i).weightedValue());
            ranges[i] = rangesEnd - 1;
        }
        Map<GossipEntry, Boolean> results = new IdentityHashMap<>();
        for (int i = 0; i < maxCount; i++) {
            int found = Arrays.binarySearch(ranges, random.nextInt(rangesEnd));
            results.put(entries.get(found < 0 ? -found - 1 : found), Boolean.TRUE);
        }
        return results.keySet();
    }

    private EntityGossips getOrCreate(UUID target) {
        return gossips.computeIfAbsent(target, uuid -> new EntityGossips());
    }

    public int transferFrom(GossipContainer source, RandomSource random, int maxCount) {
        Collection<GossipEntry> picked = source.selectGossipsForTransfer(random, maxCount);
        picked.forEach(gossip -> {
            int decayed = gossip.value - gossip.type.decayPerTransfer;
            if (decayed >= DISCARD_THRESHOLD) {
                getOrCreate(gossip.target).entries.mergeInt(gossip.type, decayed, Math::max);
            }
        });
        return picked.size();
    }

    public int getReputation(UUID entity, Predicate<GossipType> types) {
        EntityGossips entry = gossips.get(entity);
        return entry != null ? entry.weightedValue(types) : 0;
    }

    public long getCountForType(GossipType type, DoublePredicate valueTest) {
        return gossips.values().stream().filter(entry -> valueTest.test(entry.entries.getOrDefault(type, 0) * type.weight)).count();
    }

    public void add(UUID target, GossipType type, int amountToAdd) {
        EntityGossips entityGossips = getOrCreate(target);
        entityGossips.entries.mergeInt(type, amountToAdd, (old, added) -> mergeForAddition(type, old, added));
        entityGossips.makeSureValueIsntTooLowOrTooHigh(type);
        if (entityGossips.isEmpty()) {
            gossips.remove(target);
        }
    }

    public void remove(UUID target, GossipType type, int amountToRemove) {
        add(target, type, -amountToRemove);
    }

    public void remove(UUID target, GossipType type) {
        EntityGossips entityGossips = gossips.get(target);
        if (entityGossips != null) {
            entityGossips.remove(type);
            if (entityGossips.isEmpty()) {
                gossips.remove(target);
            }
        }
    }

    public void remove(GossipType type) {
        Iterator<EntityGossips> iterator = gossips.values().iterator();
        while (iterator.hasNext()) {
            EntityGossips entityGossips = iterator.next();
            entityGossips.remove(type);
            if (entityGossips.isEmpty()) {
                iterator.remove();
            }
        }
    }

    public void clear() {
        gossips.clear();
    }

    public void putAll(GossipContainer container) {
        container.gossips.forEach((target, entityGossips) -> getOrCreate(target).entries.putAll(entityGossips.entries));
    }

    private static int mergeForAddition(GossipType type, int oldValue, int added) {
        int sum = oldValue + added;
        return sum > type.max ? Math.max(type.max, oldValue) : sum;
    }

    public GossipContainer copy() {
        GossipContainer container = new GossipContainer();
        container.putAll(this);
        return container;
    }

    static record GossipEntry(UUID target, GossipType type, int value) {
        private static final Codec<UUID> UUID_CODEC = Codec.INT_STREAM.comapFlatMap(stream -> {
            int[] parts = stream.toArray();
            return parts.length == 4 ? com.mojang.serialization.DataResult.success(new UUID((long) parts[0] << 32 | (parts[1] & 0xFFFFFFFFL),
                (long) parts[2] << 32 | (parts[3] & 0xFFFFFFFFL))) : com.mojang.serialization.DataResult.error(() -> "Expected 4 ints for a UUID");
        }, uuid -> java.util.stream.IntStream.of((int) (uuid.getMostSignificantBits() >> 32), (int) uuid.getMostSignificantBits(),
            (int) (uuid.getLeastSignificantBits() >> 32), (int) uuid.getLeastSignificantBits()));
        public static final Codec<GossipEntry> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            UUID_CODEC.fieldOf("Target").forGetter(GossipEntry::target),
            GossipType.CODEC.fieldOf("Type").forGetter(GossipEntry::type),
            Codec.intRange(1, Integer.MAX_VALUE).fieldOf("Value").forGetter(GossipEntry::value)).apply(instance, GossipEntry::new));

        public int weightedValue() {
            return value * type.weight;
        }
    }

    static class EntityGossips {
        final Object2IntMap<GossipType> entries = new Object2IntOpenHashMap<>();

        public int weightedValue(Predicate<GossipType> types) {
            return entries.object2IntEntrySet().stream().filter(entry -> types.test(entry.getKey()))
                .mapToInt(entry -> entry.getIntValue() * entry.getKey().weight).sum();
        }

        public Stream<GossipEntry> unpack(UUID target) {
            return entries.object2IntEntrySet().stream().map(entry -> new GossipEntry(target, entry.getKey(), entry.getIntValue()));
        }

        public void decay() {
            ObjectIterator<Object2IntMap.Entry<GossipType>> iterator = entries.object2IntEntrySet().iterator();
            while (iterator.hasNext()) {
                Object2IntMap.Entry<GossipType> gossip = iterator.next();
                int decayed = gossip.getIntValue() - gossip.getKey().decayPerDay;
                if (decayed < DISCARD_THRESHOLD) {
                    iterator.remove();
                } else {
                    gossip.setValue(decayed);
                }
            }
        }

        public boolean isEmpty() {
            return entries.isEmpty();
        }

        public void makeSureValueIsntTooLowOrTooHigh(GossipType type) {
            int value = entries.getInt(type);
            if (value > type.max) {
                entries.put(type, type.max);
            }
            if (value < DISCARD_THRESHOLD) {
                remove(type);
            }
        }

        public void remove(GossipType type) {
            entries.removeInt(type);
        }
    }
}
