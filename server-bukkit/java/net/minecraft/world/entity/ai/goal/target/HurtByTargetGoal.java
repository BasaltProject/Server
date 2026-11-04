// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.goal.Goal;

public class HurtByTargetGoal extends TargetGoal {
    private boolean alertSameType;
    private int timestamp;
    private final Class<?>[] toIgnoreDamage;
    private Class<?>[] toIgnoreAlert;

    public HurtByTargetGoal(PathfinderMob mob, Class<?>... ignoreDamageFromTheseTypes) {
        super(mob, true);
        this.toIgnoreDamage = ignoreDamageFromTheseTypes;
        setFlags(EnumSet.of(Goal.Flag.TARGET));
    }

    @Override
    public boolean canUse() {
        int stamp = mob.getLastHurtByMobTimestamp();
        LivingEntity attacker = mob.getLastHurtByMob();
        if (stamp == timestamp || attacker == null) {
            return false;
        }
        for (Class<?> ignored : toIgnoreDamage) {
            if (ignored.isAssignableFrom(attacker.getClass())) {
                return false;
            }
        }
        return mob.canAttack(attacker);
    }

    public HurtByTargetGoal setAlertOthers(Class<?>... exceptTheseTypes) {
        alertSameType = true;
        toIgnoreAlert = exceptTheseTypes;
        return this;
    }

    @Override
    public void start() {
        mob.setTarget(mob.getLastHurtByMob());
        targetMob = mob.getTarget();
        timestamp = mob.getLastHurtByMobTimestamp();
        unseenMemoryTicks = 300;
        if (alertSameType) {
            alertOthers();
        }
        super.start();
    }

    protected void alertOthers() {
        if (!(mob.live() instanceof org.bukkit.entity.Mob live)) {
            return;
        }
        double reach = getFollowDistance();
        for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(reach, 10.0, reach)) {
            net.minecraft.world.entity.Entity handle = net.minecraft.world.entity.Entity.fromBukkit(candidate);
            if (handle != mob && handle instanceof Mob other && other.getClass() == mob.getClass()
                && other.getTarget() == null && !isIgnoredAlert(other)) {
                alertOther(other, mob.getLastHurtByMob());
            }
        }
    }

    private boolean isIgnoredAlert(Mob other) {
        if (toIgnoreAlert != null) {
            for (Class<?> ignored : toIgnoreAlert) {
                if (other.getClass() == ignored) {
                    return true;
                }
            }
        }
        return false;
    }

    protected void alertOther(Mob other, LivingEntity hurtByMob) {
        other.setTarget(hurtByMob);
    }
}
