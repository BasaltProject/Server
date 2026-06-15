package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Rabbit;

public interface RabbitOps extends Rabbit, Cored {
    default Rabbit.Type getRabbitType() {
        int id = core().nbtInt("RabbitType");
        return switch (id) {
            case 1 -> Rabbit.Type.WHITE;
            case 2 -> Rabbit.Type.BLACK;
            case 3 -> Rabbit.Type.BLACK_AND_WHITE;
            case 4 -> Rabbit.Type.GOLD;
            case 5 -> Rabbit.Type.SALT_AND_PEPPER;
            case 99 -> Rabbit.Type.THE_KILLER_BUNNY;
            default -> Rabbit.Type.BROWN;
        };
    }

    default void setRabbitType(Rabbit.Type type) {
        int id = switch (type) {
            case BROWN -> 0;
            case WHITE -> 1;
            case BLACK -> 2;
            case BLACK_AND_WHITE -> 3;
            case GOLD -> 4;
            case SALT_AND_PEPPER -> 5;
            case THE_KILLER_BUNNY -> 99;
        };
        core().nbtSet("RabbitType", "int", id);
    }
}
