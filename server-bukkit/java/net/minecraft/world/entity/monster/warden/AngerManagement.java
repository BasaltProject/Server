// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import it.unimi.dsi.fastutil.objects.ObjectIterator;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Predicate;
import net.minecraft.core.UUIDUtil;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;

public class AngerManagement {
    protected static final int CONVERSION_DELAY = 2;
    protected static final int MAX_ANGER = 150;
    private static final Codec<Pair<UUID, Integer>> SUSPECT_ANGER_PAIR = RecordCodecBuilder.create(instance -> instance.group(
        UUIDUtil.CODEC.fieldOf("uuid").forGetter(Pair::getFirst),
        ExtraCodecs.NON_NEGATIVE_INT.fieldOf("anger").forGetter(Pair::getSecond)).apply(instance, Pair::of));

    private int conversionDelay = (int) (Math.random() * 3);
    private int highestAnger;
    private final Predicate<Entity> filter;
    protected final ArrayList<Entity> suspects = new ArrayList<>();
    private final Sorter suspectSorter = new Sorter(this);
    protected final Object2IntMap<Entity> angerBySuspect = new Object2IntOpenHashMap<>();
    protected final Object2IntMap<UUID> angerByUuid;

    public static Codec<AngerManagement> codec(Predicate<Entity> filter) {
        return RecordCodecBuilder.create(instance -> instance.group(
            SUSPECT_ANGER_PAIR.listOf().lenientOptionalFieldOf("suspects", List.of()).forGetter(AngerManagement::pairs))
            .apply(instance, list -> new AngerManagement(filter, list)));
    }

    public AngerManagement(Predicate<Entity> filter, List<Pair<UUID, Integer>> angerByUuid) {
        this.filter = filter;
        this.angerByUuid = new Object2IntOpenHashMap<>(angerByUuid.size());
        angerByUuid.forEach(pair -> this.angerByUuid.put(pair.getFirst(), pair.getSecond().intValue()));
    }

    private List<Pair<UUID, Integer>> pairs() {
        List<Pair<UUID, Integer>> result = new ArrayList<>();
        suspects.forEach(entity -> result.add(Pair.of(entity.getUUID(), angerBySuspect.getInt(entity))));
        angerByUuid.object2IntEntrySet().forEach(entry -> result.add(Pair.of(entry.getKey(), entry.getIntValue())));
        return result;
    }

    public void tick(ServerLevel level, Predicate<Entity> validEntity) {
        if (--conversionDelay <= 0) {
            resolveUuids(level);
            conversionDelay = CONVERSION_DELAY;
        }
        ObjectIterator<Object2IntMap.Entry<UUID>> stored = angerByUuid.object2IntEntrySet().iterator();
        while (stored.hasNext()) {
            Object2IntMap.Entry<UUID> entry = stored.next();
            if (entry.getIntValue() <= 1) {
                stored.remove();
            } else {
                entry.setValue(entry.getIntValue() - 1);
            }
        }
        ObjectIterator<Object2IntMap.Entry<Entity>> live = angerBySuspect.object2IntEntrySet().iterator();
        while (live.hasNext()) {
            Object2IntMap.Entry<Entity> entry = live.next();
            int anger = entry.getIntValue();
            Entity entity = entry.getKey();
            Entity.RemovalReason reason = entity.getRemovalReason();
            if (anger > 1 && reason == null && validEntity.test(entity)) {
                entry.setValue(anger - 1);
                continue;
            }
            suspects.remove(entity);
            live.remove();
            boolean keepForLater = reason == Entity.RemovalReason.CHANGED_DIMENSION
                || reason == Entity.RemovalReason.UNLOADED_TO_CHUNK || reason == Entity.RemovalReason.UNLOADED_WITH_PLAYER;
            if (anger > 1 && keepForLater) {
                angerByUuid.put(entity.getUUID(), anger - 1);
            }
        }
        resort();
    }

    private void resort() {
        highestAnger = 0;
        suspects.sort(suspectSorter);
        if (suspects.size() == 1) {
            highestAnger = angerBySuspect.getInt(suspects.get(0));
        }
    }

    private void resolveUuids(ServerLevel level) {
        ObjectIterator<Object2IntMap.Entry<UUID>> iterator = angerByUuid.object2IntEntrySet().iterator();
        while (iterator.hasNext()) {
            Object2IntMap.Entry<UUID> entry = iterator.next();
            Entity entity = level.getEntity(entry.getKey());
            if (entity != null) {
                angerBySuspect.put(entity, entry.getIntValue());
                suspects.add(entity);
                iterator.remove();
            }
        }
    }

    public int increaseAnger(Entity entity, int increment) {
        boolean isNew = !angerBySuspect.containsKey(entity);
        int anger = Math.min(MAX_ANGER, angerBySuspect.getInt(entity) + increment);
        angerBySuspect.put(entity, anger);
        if (isNew) {
            anger += angerByUuid.removeInt(entity.getUUID());
            angerBySuspect.put(entity, anger);
            suspects.add(entity);
        }
        resort();
        return anger;
    }

    public void clearAnger(Entity entity) {
        angerBySuspect.removeInt(entity);
        suspects.remove(entity);
        resort();
    }

    public int getActiveAnger(Entity currentTarget) {
        return currentTarget == null ? highestAnger : angerBySuspect.getInt(currentTarget);
    }

    public Optional<LivingEntity> getActiveEntity() {
        return suspects.stream().filter(filter).findFirst().filter(entity -> entity instanceof LivingEntity).map(entity -> (LivingEntity) entity);
    }

    protected static record Sorter(AngerManagement angerManagement) implements Comparator<Entity> {
        public int compare(Entity first, Entity second) {
            if (first.equals(second)) {
                return 0;
            }
            int angerFirst = angerManagement.angerBySuspect.getOrDefault(first, 0);
            int angerSecond = angerManagement.angerBySuspect.getOrDefault(second, 0);
            angerManagement.highestAnger = Math.max(angerManagement.highestAnger, Math.max(angerFirst, angerSecond));
            boolean firstAngry = AngerLevel.byAnger(angerFirst).isAngry();
            if (firstAngry != AngerLevel.byAnger(angerSecond).isAngry()) {
                return firstAngry ? -1 : 1;
            }
            boolean firstPlayer = first instanceof Player;
            if (firstPlayer != second instanceof Player) {
                return firstPlayer ? -1 : 1;
            }
            return Integer.compare(angerSecond, angerFirst);
        }
    }
}
