package org.basaltmc.bukkit;

import org.bukkit.entity.SpawnCategory;

final class Spawning {
    private Spawning() {}

    static int limit(SpawnCategory category) {
        return switch (category) {
            case MONSTER -> 70;
            case ANIMAL -> 10;
            case WATER_ANIMAL -> 5;
            case WATER_AMBIENT -> 20;
            case WATER_UNDERGROUND_CREATURE -> 5;
            case AMBIENT -> 15;
            case AXOLOTL -> 5;
            default -> -1;
        };
    }

    static int ticksPer(SpawnCategory category) {
        return category == SpawnCategory.ANIMAL ? 400 : category == SpawnCategory.MISC ? 0 : 1;
    }
}
