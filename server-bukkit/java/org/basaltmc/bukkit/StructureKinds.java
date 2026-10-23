package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.generator.structure.Structure;
import org.bukkit.generator.structure.StructureType;

/** Which kind of structure each structure is, as the game's structure definitions say. */
final class StructureKinds {
    private StructureKinds() {}

    static String typeOf(String name) {
        if (name.startsWith("village_") || name.startsWith("abandoned_camp_")) {
            return "jigsaw";
        }
        if (name.startsWith("mineshaft")) {
            return "mineshaft";
        } else if (name.startsWith("ocean_ruin")) {
            return "ocean_ruin";
        } else if (name.startsWith("ruined_portal")) {
            return "ruined_portal";
        } else if (name.startsWith("shipwreck")) {
            return "shipwreck";
        }
        return switch (name) {
            case "pillager_outpost", "ancient_city", "bastion_remnant", "trail_ruins", "trial_chambers" -> "jigsaw";
            case "mansion" -> "woodland_mansion";
            case "monument" -> "ocean_monument";
            case "jungle_pyramid" -> "jungle_temple";
            default -> name;
        };
    }

    static StructureType typeOf(NamespacedKey key) {
        return Registry.STRUCTURE_TYPE.get(NamespacedKey.minecraft(typeOf(key.getKey())));
    }

    static List<Structure> structuresOf(StructureType type) {
        List<Structure> out = new ArrayList<>();
        for (Structure structure : Registry.STRUCTURE) {
            if (typeOf(structure.getKey().getKey()).equals(type.getKey().getKey())) {
                out.add(structure);
            }
        }
        return out;
    }
}
