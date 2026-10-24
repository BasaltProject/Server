package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Warden;

public interface WardenOps extends Warden, Cored {
    private int angerOf(Entity entity) {
        if (entity == null) {
            throw new IllegalArgumentException("entity cannot be null");
        }
        return entity.getEntityId();
    }

    default int getAnger() {
        return getHighestAnger();
    }

    default int getAnger(Entity entity) {
        return core().call("warden_anger", (long) angerOf(entity)) instanceof Number level ? level.intValue() : 0;
    }

    default int getHighestAnger() {
        return core().integer("warden_highest_anger");
    }

    default void increaseAnger(Entity entity, int increase) {
        core().call("warden_increase_anger", (long) angerOf(entity), (long) increase);
    }

    default void setAnger(Entity entity, int anger) {
        core().call("warden_set_anger", (long) angerOf(entity), (long) anger);
    }

    default void clearAnger(Entity entity) {
        core().call("warden_clear_anger", (long) angerOf(entity));
    }

    default LivingEntity getEntityAngryAt() {
        return core().entity(core().integer("warden_angry_at")) instanceof LivingEntity living ? living : null;
    }

    default void setDisturbanceLocation(Location location) {
        if (location == null || !core().worldRef().equals(location.getWorld())) {
            throw new IllegalArgumentException("The disturbance must be in the warden's world");
        }
        core().call("warden_disturb", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ());
    }

    default Warden.AngerLevel getAngerLevel() {
        int anger = getHighestAnger();
        return anger >= 80 ? Warden.AngerLevel.ANGRY : anger >= 40 ? Warden.AngerLevel.AGITATED
            : Warden.AngerLevel.CALM;
    }
}
