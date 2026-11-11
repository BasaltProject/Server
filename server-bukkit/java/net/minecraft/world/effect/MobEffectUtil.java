// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import java.util.List;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.Mth;
import net.minecraft.util.StringUtil;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.phys.Vec3;

public final class MobEffectUtil {
    public static Component formatDuration(MobEffectInstance instance, float scale, float tickrate) {
        if (instance.isInfiniteDuration()) {
            return Component.translatable("effect.duration.infinite");
        }
        return Component.literal(StringUtil.formatTickDuration(Mth.floor(instance.getDuration() * scale), tickrate));
    }

    public static boolean hasDigSpeed(LivingEntity mob) {
        return mob.hasEffect(MobEffects.HASTE) || mob.hasEffect(MobEffects.CONDUIT_POWER);
    }

    public static int getDigSpeedAmplification(LivingEntity mob) {
        int haste = mob.hasEffect(MobEffects.HASTE) ? mob.getEffect(MobEffects.HASTE).getAmplifier() : 0;
        int conduit = mob.hasEffect(MobEffects.CONDUIT_POWER) ? mob.getEffect(MobEffects.CONDUIT_POWER).getAmplifier() : 0;
        return Math.max(haste, conduit);
    }

    public static boolean hasWaterBreathing(LivingEntity mob) {
        return mob.hasEffect(MobEffects.WATER_BREATHING) || mob.hasEffect(MobEffects.CONDUIT_POWER)
            || mob.hasEffect(MobEffects.BREATH_OF_THE_NAUTILUS);
    }

    public static boolean shouldEffectsRefillAirsupply(LivingEntity mob) {
        return !mob.hasEffect(MobEffects.BREATH_OF_THE_NAUTILUS) || mob.hasEffect(MobEffects.WATER_BREATHING)
            || mob.hasEffect(MobEffects.CONDUIT_POWER);
    }

    public static List<ServerPlayer> addEffectToPlayersAround(ServerLevel level, Entity source, Vec3 position, double radius,
        MobEffectInstance effectInstance, int displayEffectLimit) {
        var effect = effectInstance.getEffect();
        List<ServerPlayer> targets = level.getPlayers(candidate -> {
            if (!candidate.gameMode.isSurvival() || (source != null && source.isAlliedTo(candidate))) {
                return false;
            }
            if (!position.closerThan(candidate.position(), radius)) {
                return false;
            }
            var current = candidate.getEffect(effect);
            return current == null || current.getAmplifier() < effectInstance.getAmplifier() || current.endsWithin(displayEffectLimit - 1);
        });
        targets.forEach(player -> player.addEffect(new MobEffectInstance(effectInstance), source));
        return targets;
    }
}
