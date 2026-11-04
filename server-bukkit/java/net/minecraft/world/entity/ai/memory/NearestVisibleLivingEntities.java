// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.memory;

import java.util.List;
import java.util.Optional;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;

public class NearestVisibleLivingEntities {
    private static final NearestVisibleLivingEntities kEmpty = new NearestVisibleLivingEntities();
    private final List<LivingEntity> nearbyEntities;
    private final Predicate<LivingEntity> lineOfSightTest;

    private NearestVisibleLivingEntities() {
        nearbyEntities = List.of();
        lineOfSightTest = entity -> false;
    }

    public NearestVisibleLivingEntities(ServerLevel level, LivingEntity body, List<LivingEntity> livingEntities) {
        nearbyEntities = livingEntities;
        java.util.Map<LivingEntity, Boolean> cache = new java.util.IdentityHashMap<>();
        lineOfSightTest = entity -> cache.computeIfAbsent(entity, key -> net.minecraft.world.entity.ai.sensing.Sensor.isEntityTargetable(level, body, key));
    }

    public static NearestVisibleLivingEntities empty() {
        return kEmpty;
    }

    public List<LivingEntity> nearbyEntities() {
        return nearbyEntities;
    }

    public Optional<LivingEntity> findClosest(Predicate<LivingEntity> filter) {
        for (LivingEntity entity : nearbyEntities) {
            if (filter.test(entity) && lineOfSightTest.test(entity)) {
                return Optional.of(entity);
            }
        }
        return Optional.empty();
    }

    public Iterable<LivingEntity> findAll(Predicate<LivingEntity> filter) {
        return find(filter).toList();
    }

    public Stream<LivingEntity> find(Predicate<LivingEntity> filter) {
        return nearbyEntities.stream().filter(entity -> filter.test(entity) && lineOfSightTest.test(entity));
    }

    public boolean contains(LivingEntity targetEntity) {
        return nearbyEntities.contains(targetEntity) && lineOfSightTest.test(targetEntity);
    }

    public boolean contains(Predicate<LivingEntity> filter) {
        for (LivingEntity entity : nearbyEntities) {
            if (filter.test(entity) && lineOfSightTest.test(entity)) {
                return true;
            }
        }
        return false;
    }
}
