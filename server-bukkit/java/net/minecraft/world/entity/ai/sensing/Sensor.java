// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Comparator;
import java.util.Set;
import java.util.function.BiPredicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public abstract class Sensor<E extends LivingEntity> {
    private static final int kDefaultScanRate = 20;
    protected static final int kTargetingRange = 16;
    private static final TargetingConditions kTargetConditions = TargetingConditions.forNonCombat().range(16.0);
    private static final TargetingConditions kTargetConditionsIgnoreInvisibility = TargetingConditions.forNonCombat().range(16.0)
        .ignoreInvisibilityTesting();
    private static final TargetingConditions kAttackTargetConditions = TargetingConditions.forCombat().range(16.0);
    private static final TargetingConditions kAttackTargetConditionsIgnoreInvisibility = TargetingConditions.forCombat().range(16.0)
        .ignoreInvisibilityTesting();
    private static final TargetingConditions kAttackTargetConditionsIgnoreLineOfSight = TargetingConditions.forCombat().range(16.0)
        .ignoreLineOfSight();
    private static final TargetingConditions kAttackTargetConditionsIgnoreInvisibilityAndLineOfSight = TargetingConditions.forCombat()
        .range(16.0).ignoreLineOfSight().ignoreInvisibilityTesting();

    private final int scanRate;
    private long timeToTick;

    public Sensor(int scanRate) {
        this.scanRate = scanRate;
        this.timeToTick = RandomSource.create().nextInt(scanRate);
    }

    public Sensor() {
        this(kDefaultScanRate);
    }

    public void randomlyDelayStart(RandomSource randomSource) {
        timeToTick = randomSource.nextInt(scanRate);
    }

    public final void tick(ServerLevel level, E body) {
        if (--timeToTick <= 0L) {
            timeToTick = scanRate;
            doTick(level, body);
        }
    }

    protected abstract void doTick(ServerLevel level, E body);

    public abstract Set<MemoryModuleType<?>> requires();

    public static boolean isEntityTargetable(ServerLevel level, LivingEntity body, LivingEntity entity) {
        return body.getBrain().isMemoryValue(MemoryModuleType.ATTACK_TARGET, entity)
            ? kTargetConditionsIgnoreInvisibility.test(level, body, entity) : kTargetConditions.test(level, body, entity);
    }

    public static boolean isEntityAttackable(ServerLevel level, LivingEntity body, LivingEntity target) {
        return body.getBrain().isMemoryValue(MemoryModuleType.ATTACK_TARGET, target)
            ? kAttackTargetConditionsIgnoreInvisibility.test(level, body, target) : kAttackTargetConditions.test(level, body, target);
    }

    public static BiPredicate<ServerLevel, LivingEntity> wasEntityAttackableLastNTicks(LivingEntity body, int ticks) {
        return rememberPositives(ticks, (level, target) -> isEntityAttackable(level, body, target));
    }

    public static boolean isEntityAttackableIgnoringLineOfSight(ServerLevel level, LivingEntity body, LivingEntity target) {
        return body.getBrain().isMemoryValue(MemoryModuleType.ATTACK_TARGET, target)
            ? kAttackTargetConditionsIgnoreInvisibilityAndLineOfSight.test(level, body, target)
            : kAttackTargetConditionsIgnoreLineOfSight.test(level, body, target);
    }

    public static <T, U> BiPredicate<T, U> rememberPositives(int invocations, BiPredicate<T, U> predicate) {
        int[] remaining = {0};
        return (first, second) -> {
            if (predicate.test(first, second)) {
                remaining[0] = invocations;
                return true;
            }
            return remaining[0]-- > 0;
        };
    }
}
