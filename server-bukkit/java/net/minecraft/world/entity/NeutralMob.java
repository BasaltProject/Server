// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.Optional;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public interface NeutralMob {
    String TAG_ANGER_END_TIME = "anger_end_time";
    String TAG_ANGRY_AT = "angry_at";
    long NO_ANGER_END_TIME = -1L;

    long getPersistentAngerEndTime();

    default void setTimeToRemainAngry(long remainingTime) {
        setPersistentAngerEndTime(level().getGameTime() + remainingTime);
    }

    void setPersistentAngerEndTime(long endTime);

    EntityReference<LivingEntity> getPersistentAngerTarget();

    void setPersistentAngerTarget(EntityReference<LivingEntity> persistentAngerTarget);

    void startPersistentAngerTimer();

    Level level();

    default void addPersistentAngerSaveData(ValueOutput output) {
        output.putLong(TAG_ANGER_END_TIME, getPersistentAngerEndTime());
        output.storeNullable(TAG_ANGRY_AT, EntityReference.codec(), getPersistentAngerTarget());
    }

    default void readPersistentAngerSaveData(Level level, ValueInput input) {
        Optional<Long> endTime = input.getLong(TAG_ANGER_END_TIME);
        if (endTime.isPresent()) {
            setPersistentAngerEndTime(endTime.get());
        } else {
            Optional<Integer> legacyTime = input.getInt("AngerTime");
            if (legacyTime.isPresent()) {
                setTimeToRemainAngry(legacyTime.get());
            } else {
                setPersistentAngerEndTime(NO_ANGER_END_TIME);
            }
        }
        if (level instanceof ServerLevel) {
            setPersistentAngerTarget(EntityReference.read(input, TAG_ANGRY_AT));
            setTarget(EntityReference.getLivingEntity(getPersistentAngerTarget(), level));
        }
    }

    default void updatePersistentAnger(ServerLevel level, boolean stayAngryIfTargetPresent) {
        LivingEntity previousTarget = getTargetUnchecked();
        EntityReference<LivingEntity> angerTarget = getPersistentAngerTarget();
        if (previousTarget != null && previousTarget.isDeadOrDying() && angerTarget != null && angerTarget.matches(previousTarget)
                && previousTarget instanceof Mob) {
            stopBeingAngry();
            return;
        }
        LivingEntity target = getTarget();
        if (target != null) {
            boolean changed = angerTarget == null || !angerTarget.matches(target);
            if (changed) {
                setPersistentAngerTarget(EntityReference.of(target));
            }
            if (changed || stayAngryIfTargetPresent) {
                startPersistentAngerTimer();
            }
        }
        if (angerTarget != null && !isAngry() && (target == null || !isAttackablePlayer(target) || !stayAngryIfTargetPresent)) {
            stopBeingAngry();
        }
        if (EntityReference.getLivingEntity(angerTarget, level) instanceof Player player
                && (player.isCreative() || player.isSpectator() || level.getDifficulty() == Difficulty.PEACEFUL)) {
            stopBeingAngry();
        }
    }

    private static boolean isAttackablePlayer(LivingEntity target) {
        return target instanceof Player player && !player.isCreative() && !player.isSpectator()
                && player.level().getDifficulty() != Difficulty.PEACEFUL;
    }

    default boolean isAngryAt(LivingEntity entity, ServerLevel level) {
        if (!canAttack(entity)) {
            return false;
        }
        if (isAttackablePlayer(entity) && isAngryAtAllPlayers(level)) {
            return true;
        }
        EntityReference<LivingEntity> angerTarget = getPersistentAngerTarget();
        return angerTarget != null && angerTarget.matches(entity);
    }

    default boolean isAngryAtAllPlayers(ServerLevel level) {
        Boolean universal = level.getWorld().getGameRuleValue(org.bukkit.GameRule.UNIVERSAL_ANGER);
        return Boolean.TRUE.equals(universal) && isAngry() && getPersistentAngerTarget() == null;
    }

    default boolean isAngry() {
        long endTime = getPersistentAngerEndTime();
        return endTime > 0L && endTime - level().getGameTime() > 0L;
    }

    default void playerDied(ServerLevel level, Player player) {
        Boolean forgive = level.getWorld().getGameRuleValue(org.bukkit.GameRule.FORGIVE_DEAD_PLAYERS);
        EntityReference<LivingEntity> angerTarget = getPersistentAngerTarget();
        if (Boolean.TRUE.equals(forgive) && angerTarget != null && angerTarget.matches(player)) {
            stopBeingAngry();
        }
    }

    default void forgetCurrentTargetAndRefreshUniversalAnger() {
        stopBeingAngry();
        startPersistentAngerTimer();
    }

    default void stopBeingAngry() {
        setLastHurtByMob(null);
        setPersistentAngerTarget(null);
        setTarget(null);
        setPersistentAngerEndTime(NO_ANGER_END_TIME);
    }

    LivingEntity getLastHurtByMob();

    void setLastHurtByMob(LivingEntity hurtBy);

    void setTarget(LivingEntity target);

    boolean canAttack(LivingEntity target);

    LivingEntity getTarget();

    LivingEntity getTargetUnchecked();
}
