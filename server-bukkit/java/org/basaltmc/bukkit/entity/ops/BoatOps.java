package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Boat;

public interface BoatOps extends Boat, Cored {
    default Boat.Type getBoatType() {
        String kind = core().type.getKey().getKey().replace("_chest_boat", "").replace("_boat", "")
            .replace("_chest_raft", "").replace("_raft", "").toUpperCase(java.util.Locale.ROOT);
        try {
            return Boat.Type.valueOf(kind);
        } catch (IllegalArgumentException unknown) {
            return Boat.Type.OAK;
        }
    }

    default org.bukkit.Material getBoatMaterial() {
        return getBoatType().getMaterial();
    }

    default org.bukkit.TreeSpecies getWoodType() {
        return switch (getBoatType()) {
            case SPRUCE -> org.bukkit.TreeSpecies.REDWOOD;
            case BIRCH -> org.bukkit.TreeSpecies.BIRCH;
            case JUNGLE -> org.bukkit.TreeSpecies.JUNGLE;
            case ACACIA -> org.bukkit.TreeSpecies.ACACIA;
            case DARK_OAK -> org.bukkit.TreeSpecies.DARK_OAK;
            default -> org.bukkit.TreeSpecies.GENERIC;
        };
    }

    default double getMaxSpeed() {
        return core().real("boat_max_speed");
    }

    default void setMaxSpeed(double speed) {
        if (speed >= 0.0) {
            core().set("boat_max_speed", speed);
        }
    }

    default double getOccupiedDeceleration() {
        return core().real("boat_occupied_deceleration");
    }

    default void setOccupiedDeceleration(double rate) {
        if (rate >= 0.0) {
            core().set("boat_occupied_deceleration", rate);
        }
    }

    default double getUnoccupiedDeceleration() {
        return core().real("boat_unoccupied_deceleration");
    }

    default void setUnoccupiedDeceleration(double rate) {
        core().set("boat_unoccupied_deceleration", rate);
    }

    default boolean getWorkOnLand() {
        return core().flag("boat_work_on_land");
    }

    default void setWorkOnLand(boolean workOnLand) {
        core().set("boat_work_on_land", workOnLand);
    }

    default Boat.Status getStatus() {
        return Boat.Status.values()[Math.max(0, Math.min(4, core().integer("boat_status")))];
    }

    default void setBoatType(Boat.Type type) {
        String current = core().type.getKey().getKey();
        boolean chest = current.contains("chest");
        boolean raft = type == Boat.Type.BAMBOO;
        String wood = type.name().toLowerCase(java.util.Locale.ROOT);
        core().retype(wood + (chest ? (raft ? "_chest_raft" : "_chest_boat") : (raft ? "_raft" : "_boat")));
    }

    default void setWoodType(org.bukkit.TreeSpecies species) {
        setBoatType(switch (species) {
            case REDWOOD -> Boat.Type.SPRUCE;
            case BIRCH -> Boat.Type.BIRCH;
            case JUNGLE -> Boat.Type.JUNGLE;
            case ACACIA -> Boat.Type.ACACIA;
            case DARK_OAK -> Boat.Type.DARK_OAK;
            default -> Boat.Type.OAK;
        });
    }
}
