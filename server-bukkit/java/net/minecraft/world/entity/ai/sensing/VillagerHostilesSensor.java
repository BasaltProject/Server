// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import com.google.common.collect.ImmutableMap;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EntityTypes;
public class VillagerHostilesSensor extends NearestVisibleLivingEntitySensor {
    private static final ImmutableMap<EntityType<?>, Float> kRange = ImmutableMap.<EntityType<?>, Float>builder()
        .put(EntityTypes.DROWNED, 8.0F).put(EntityTypes.EVOKER, 12.0F).put(EntityTypes.HUSK, 8.0F).put(EntityTypes.ILLUSIONER, 12.0F)
        .put(EntityTypes.PILLAGER, 15.0F).put(EntityTypes.RAVAGER, 12.0F).put(EntityTypes.VEX, 8.0F).put(EntityTypes.VINDICATOR, 10.0F)
        .put(EntityTypes.ZOGLIN, 10.0F).put(EntityTypes.ZOMBIE, 8.0F).put(EntityTypes.ZOMBIE_VILLAGER, 8.0F).build();
    @Override
    protected boolean isMatchingEntity(ServerLevel level, LivingEntity body, LivingEntity mob) {
        Float range = kRange.get(mob.getType());
        return range != null && mob.distanceToSqr(body) <= range * range;
    }
    @Override
    protected MemoryModuleType<LivingEntity> getMemoryToSet() {
        return MemoryModuleType.NEAREST_HOSTILE;
    }
}
