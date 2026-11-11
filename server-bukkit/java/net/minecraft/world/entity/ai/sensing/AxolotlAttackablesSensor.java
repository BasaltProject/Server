// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import com.google.common.collect.Sets;
import net.minecraft.tags.EntityTypeTags;
public class AxolotlAttackablesSensor extends NearestVisibleLivingEntitySensor {
    public static final float TARGET_DETECTION_DISTANCE = 8.0F;
    @Override
    protected boolean isMatchingEntity(ServerLevel level, LivingEntity body, LivingEntity mob) {
        boolean hunted = !body.getBrain().hasMemoryValue(MemoryModuleType.HAS_HUNTING_COOLDOWN) && mob.is(EntityTypeTags.AXOLOTL_HUNT_TARGETS);
        boolean hostile = mob.is(EntityTypeTags.AXOLOTL_ALWAYS_HOSTILES);
        return mob.distanceToSqr(body) <= 64.0 && mob.isInWater() && (hostile || hunted) && Sensor.isEntityAttackable(level, body, mob);
    }
    @Override
    protected MemoryModuleType<LivingEntity> getMemoryToSet() {
        return MemoryModuleType.NEAREST_ATTACKABLE;
    }
    @Override
    public Set<MemoryModuleType<?>> requires() {
        return Sets.union(super.requires(), Set.of(MemoryModuleType.HAS_HUNTING_COOLDOWN));
    }
}
