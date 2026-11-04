// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Optional;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.animal.Animal;

public class AnimalMakeLove extends Behavior<Animal> {
    private static final int BREED_RANGE = 3;
    private static final int DEFAULT_CLOSE_ENOUGH_DISTANCE = 2;
    private final EntityType<? extends Animal> partnerType;
    private final float speedModifier;
    private final int closeEnoughDistance;
    private long spawnChildAtTime;

    public AnimalMakeLove(EntityType<? extends Animal> partnerType) {
        this(partnerType, 1.0F, DEFAULT_CLOSE_ENOUGH_DISTANCE);
    }

    public AnimalMakeLove(EntityType<? extends Animal> partnerType, float speedModifier, int closeEnoughDistance) {
        super(Map.of(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, MemoryStatus.VALUE_PRESENT, MemoryModuleType.BREED_TARGET, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED,
            MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT), 110);
        this.partnerType = partnerType;
        this.speedModifier = speedModifier;
        this.closeEnoughDistance = closeEnoughDistance;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Animal body) {
        return body.isInLove() && findValidBreedPartner(body).isPresent();
    }

    protected void start(ServerLevel level, Animal body, long timestamp) {
        Animal partner = findValidBreedPartner(body).get();
        body.getBrain().setMemory(MemoryModuleType.BREED_TARGET, partner);
        partner.getBrain().setMemory(MemoryModuleType.BREED_TARGET, body);
        BehaviorUtils.lockGazeAndWalkToEachOther(body, partner, speedModifier, closeEnoughDistance);
        spawnChildAtTime = timestamp + 60 + body.getRandom().nextInt(50);
    }

    protected boolean canStillUse(ServerLevel level, Animal body, long timestamp) {
        if (!hasBreedTargetOfRightType(body)) {
            return false;
        }
        Animal partner = breedTarget(body);
        return partner.isAlive() && body.canMate(partner) && BehaviorUtils.entityIsVisible(body.getBrain(), partner)
            && timestamp <= spawnChildAtTime && !body.isPanicking() && !partner.isPanicking();
    }

    protected void tick(ServerLevel level, Animal body, long timestamp) {
        Animal partner = breedTarget(body);
        BehaviorUtils.lockGazeAndWalkToEachOther(body, partner, speedModifier, closeEnoughDistance);
        if (body.closerThan(partner, BREED_RANGE) && timestamp >= spawnChildAtTime) {
            body.spawnChildFromBreeding(level, partner);
            body.getBrain().eraseMemory(MemoryModuleType.BREED_TARGET);
            partner.getBrain().eraseMemory(MemoryModuleType.BREED_TARGET);
        }
    }

    protected void stop(ServerLevel level, Animal body, long timestamp) {
        body.getBrain().eraseMemory(MemoryModuleType.BREED_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
        spawnChildAtTime = 0L;
    }

    private Animal breedTarget(Animal body) {
        return (Animal) body.getBrain().getMemory(MemoryModuleType.BREED_TARGET).get();
    }

    private boolean hasBreedTargetOfRightType(Animal body) {
        Optional<? extends net.minecraft.world.entity.AgeableMob> target = body.getBrain().getMemory(MemoryModuleType.BREED_TARGET);
        return target.isPresent() && target.get().is(partnerType);
    }

    private Optional<? extends Animal> findValidBreedPartner(Animal body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES).flatMap(visible -> visible.findClosest(
            entity -> entity.is(partnerType) && entity instanceof Animal animal && body.canMate(animal) && !animal.isPanicking())).map(Animal.class::cast);
    }
}
