// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import java.util.function.ToIntFunction;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.Silverfish;
import net.minecraft.world.phys.Vec3;

class InfestedMobEffect extends MobEffect {
    private final float chanceToSpawn;
    private final ToIntFunction<RandomSource> spawnedCount;

    protected InfestedMobEffect(MobEffectCategory category, int color, float chanceToSpawn, ToIntFunction<RandomSource> spawnedCount) {
        super(category, color, ParticleTypes.INFESTED);
        this.chanceToSpawn = chanceToSpawn;
        this.spawnedCount = spawnedCount;
    }

    @Override
    public void onMobHurt(ServerLevel level, LivingEntity mob, int amplifier, DamageSource source, float damage) {
        if (mob.getRandom().nextFloat() > chanceToSpawn) {
            return;
        }
        int wanted = spawnedCount.applyAsInt(mob.getRandom());
        for (int n = 0; n < wanted; n++) {
            release(level, mob);
        }
    }

    private void release(ServerLevel level, LivingEntity mob) {
        Silverfish fish = (Silverfish) EntityTypes.SILVERFISH.create(level, EntitySpawnReason.TRIGGERED);
        if (fish == null) {
            return;
        }
        float spread = Mth.randomBetween(mob.getRandom(), -1.5707964F, 1.5707964F);
        var push = mob.getLookAngle().toVector3f().mul(0.3F).mul(1.0F, 1.5F, 1.0F).rotateY(spread);
        fish.snapTo(mob.getX(), mob.getY() + mob.getBbHeight() / 2.0, mob.getZ(), level.getRandom().nextFloat() * 360.0F, 0.0F);
        fish.setDeltaMovement(new Vec3(push));
        level.addFreshEntity(fish);
        fish.playSound(SoundEvents.SILVERFISH_HURT);
    }
}
