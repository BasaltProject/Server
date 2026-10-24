package org.basaltmc.bukkit;

import io.papermc.paper.world.damagesource.CombatEntry;
import io.papermc.paper.world.damagesource.FallLocationType;
import org.bukkit.damage.DamageSource;

/** One damage a living entity took, as the combat tracker keeps it. */
record BasaltCombatEntry(DamageSource source, float damage, FallLocationType location, float distance)
        implements CombatEntry {
    @Override
    public DamageSource getDamageSource() {
        return source;
    }

    @Override
    public float getDamage() {
        return damage;
    }

    @Override
    public FallLocationType getFallLocationType() {
        return location;
    }

    @Override
    public float getFallDistance() {
        return distance;
    }
}
