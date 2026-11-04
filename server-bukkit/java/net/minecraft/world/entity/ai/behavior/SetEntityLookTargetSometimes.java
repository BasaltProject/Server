// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;

public class SetEntityLookTargetSometimes {
    public static BehaviorControl<LivingEntity> create(float maxDist, UniformInt interval) {
        return create(entity -> true, maxDist, interval);
    }

    public static BehaviorControl<LivingEntity> create(EntityType<?> type, float maxDist, UniformInt interval) {
        return create(entity -> type.equals(entity.getType()), maxDist, interval);
    }

    private static BehaviorControl<LivingEntity> create(java.util.function.Predicate<LivingEntity> predicate, float maxDist, UniformInt interval) {
        Ticker ticker = new Ticker(interval);
        OneShot<LivingEntity> inner = SetEntityLookTarget.create(predicate, maxDist);
        return OneShots.of(inner.getRequiredMemories(), (level, body, timestamp) -> ticker.tickDownAndCheck(level.getRandom())
            && inner.trigger(level, body, timestamp));
    }

    public static final class Ticker {
        private final UniformInt interval;
        private int ticksUntilNext;

        public Ticker(UniformInt interval) {
            this.interval = interval;
        }

        public boolean tickDownAndCheck(RandomSource random) {
            if (ticksUntilNext > 0) {
                ticksUntilNext--;
                return false;
            }
            ticksUntilNext = interval.sample(random) - 1;
            return true;
        }
    }
}
