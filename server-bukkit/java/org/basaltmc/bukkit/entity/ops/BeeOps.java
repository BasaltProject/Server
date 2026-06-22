package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Bee;

public interface BeeOps extends Bee, Cored {
    private Location position(String key) {
        return EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get(key));
    }

    private void position(String key, Location location) {
        if (location == null) {
            core().nbtRemove(key);
            return;
        }
        if (!core().worldRef().equals(location.getWorld())) {
            throw new IllegalArgumentException("Position must be in the same world");
        }
        core().nbtSet(key, "pos", location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ());
    }

    default Location getHive() {
        return position("hive_pos");
    }

    default void setHive(Location location) {
        position("hive_pos", location);
    }

    default Location getFlower() {
        return position("flower_pos");
    }

    default void setFlower(Location location) {
        position("flower_pos", location);
    }

    default boolean hasNectar() {
        return core().nbtFlag("HasNectar");
    }

    default void setHasNectar(boolean nectar) {
        core().nbtSet("HasNectar", "bool", nectar);
    }

    default boolean hasStung() {
        return core().nbtFlag("HasStung");
    }

    default void setHasStung(boolean stung) {
        core().nbtSet("HasStung", "bool", stung);
    }

    default int getAnger() {
        long end = core().nbtValue("anger_end_time") instanceof Number number ? number.longValue() : 0L;
        return (int) Math.max(end - getWorld().getGameTime(), 0L);
    }

    default void setAnger(int anger) {
        core().nbtSet("anger_end_time", "long", getWorld().getGameTime() + anger);
    }

    default int getCannotEnterHiveTicks() {
        return core().nbtInt("CannotEnterHiveTicks");
    }

    default void setCannotEnterHiveTicks(int ticks) {
        core().nbtSet("CannotEnterHiveTicks", "int", ticks);
    }

    default int getCropsGrownSincePollination() {
        return core().nbtInt("CropsGrownSincePollination");
    }

    default void setCropsGrownSincePollination(int crops) {
        core().nbtSet("CropsGrownSincePollination", "int", crops);
    }

    default int getTicksSincePollination() {
        return core().nbtInt("TicksSincePollination");
    }

    default void setTicksSincePollination(int ticks) {
        core().nbtSet("TicksSincePollination", "int", ticks);
    }
}
