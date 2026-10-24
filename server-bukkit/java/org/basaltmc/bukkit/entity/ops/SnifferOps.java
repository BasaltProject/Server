package org.basaltmc.bukkit.entity.ops;

import java.util.ArrayList;
import java.util.Collection;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Sniffer;

public interface SnifferOps extends Sniffer, Cored {
    default Collection<Location> getExploredLocations() {
        Collection<Location> out = new ArrayList<>();
        if (core().call("sniffer_explored") instanceof String[] flat) {
            for (String position : flat) {
                out.add(MobPositions.parse(core(), position));
            }
        }
        return out;
    }

    default void addExploredLocation(Location location) {
        explore(location, false);
    }

    default void removeExploredLocation(Location location) {
        explore(location, true);
    }

    private void explore(Location location, boolean remove) {
        core().call("sniffer_explore", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), remove);
    }

    default Sniffer.State getState() {
        return Sniffer.State.values()[Math.max(0, Math.min(Sniffer.State.values().length - 1,
            core().integer("sniffer_state")))];
    }

    default void setState(Sniffer.State state) {
        core().call("sniffer_set_state", (long) state.ordinal());
    }

    default boolean canDig() {
        return core().test("sniffer_can_dig");
    }

    default Location findPossibleDigLocation() {
        return MobPositions.parse(core(), core().call("sniffer_dig_spot") instanceof String spot ? spot : null);
    }
}
