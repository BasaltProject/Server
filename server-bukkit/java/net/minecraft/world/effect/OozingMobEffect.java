// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import java.util.ArrayList;
import java.util.List;
import java.util.function.ToIntFunction;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.cubemob.Slime;
import net.minecraft.world.level.gamerules.GameRules;

class OozingMobEffect extends MobEffect {
    public static final int SLIME_SIZE = 2;
    private final ToIntFunction<RandomSource> spawnedCount;

    protected OozingMobEffect(MobEffectCategory category, int color, ToIntFunction<RandomSource> spawnedCount) {
        super(category, color, ParticleTypes.ITEM_SLIME);
        this.spawnedCount = spawnedCount;
    }

    protected static int numberOfSlimesToSpawn(int maxEntityCramming, NearbySlimes nearbySlimes, int numberRequested) {
        if (maxEntityCramming < 1) {
            return numberRequested;
        }
        return Mth.clamp(0, maxEntityCramming - nearbySlimes.count(maxEntityCramming), numberRequested);
    }

    @Override
    public void onMobRemoved(ServerLevel level, LivingEntity mob, int amplifier, Entity.RemovalReason reason) {
        if (reason != Entity.RemovalReason.KILLED) {
            return;
        }
        int wanted = spawnedCount.applyAsInt(mob.getRandom());
        int cramming = level.getGameRules().get(GameRules.MAX_ENTITY_CRAMMING);
        int spawn = numberOfSlimesToSpawn(cramming, maxResults -> {
            List<Slime> found = new ArrayList<>();
            level.getEntities(EntityTypes.SLIME, mob.getBoundingBox().inflate(2.0), other -> other != mob, found, maxResults);
            return found.size();
        }, wanted);
        for (int n = 0; n < spawn; n++) {
            Slime slime = (Slime) EntityTypes.SLIME.create(level, EntitySpawnReason.TRIGGERED);
            if (slime != null) {
                slime.setSize(SLIME_SIZE, true);
                slime.snapTo(mob.getX(), mob.getY() + 0.5, mob.getZ(), level.getRandom().nextFloat() * 360.0F, 0.0F);
                level.addFreshEntity(slime);
            }
        }
    }

    protected interface NearbySlimes {
        int count(int maxResults);
    }
}
