// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.LivingEntity;
public class CombatTracker {
    private final LivingEntity mob;
    private boolean inCombat;
    private boolean takingDamage;
    private int lastDamageTime;
    private int combatStartTime;
    private int combatEndTime;
    private final java.util.List<DamageSource> entries = new java.util.ArrayList<>();
    public static final int RESET_DAMAGE_STATUS_TIME = 100;
    public static final int RESET_COMBAT_STATUS_TIME = 300;
    public CombatTracker(LivingEntity mob) {
        this.mob = mob;
    }
    public void recordDamage(DamageSource source, float damage) {
        recheckStatus();
        entries.add(source);
        lastDamageTime = mob.tickCount;
        takingDamage = true;
        if (!inCombat && mob.isAlive()) {
            inCombat = true;
            combatStartTime = mob.tickCount;
            combatEndTime = combatStartTime;
        }
    }
    public Component getDeathMessage() {
        if (entries.isEmpty()) {
            return Component.translatable("death.attack.generic", mob.getDisplayName());
        }
        return entries.get(entries.size() - 1).getLocalizedDeathMessage(mob);
    }
    public int getCombatDuration() {
        return (inCombat ? mob.tickCount : combatEndTime) - combatStartTime;
    }
    public void recheckStatus() {
        int window = inCombat ? RESET_COMBAT_STATUS_TIME : RESET_DAMAGE_STATUS_TIME;
        if (takingDamage && (!mob.isAlive() || mob.tickCount - lastDamageTime > window)) {
            takingDamage = false;
            inCombat = false;
            combatEndTime = mob.tickCount;
            entries.clear();
        }
    }
}
