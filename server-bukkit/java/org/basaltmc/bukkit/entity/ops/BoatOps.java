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
}
