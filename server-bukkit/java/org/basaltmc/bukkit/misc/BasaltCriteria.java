package org.basaltmc.bukkit.misc;

import org.bukkit.scoreboard.Criteria;
import org.bukkit.scoreboard.RenderType;

public record BasaltCriteria(String name) implements Criteria {
    @Override
    public String getName() {
        return name;
    }

    @Override
    public boolean isReadOnly() {
        return switch (name) {
            case "health", "food", "air", "armor", "xp", "level" -> true;
            default -> false;
        };
    }

    @Override
    public RenderType getDefaultRenderType() {
        return name.equals("health") ? RenderType.HEARTS : RenderType.INTEGER;
    }
}
