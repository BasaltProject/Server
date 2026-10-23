package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Interaction;

public interface InteractionOps extends Interaction, Cored {
    default float getInteractionWidth() {
        return (float) core().nbtReal("width");
    }

    default void setInteractionWidth(float width) {
        core().nbtSet("width", "float", (double) width);
    }

    default float getInteractionHeight() {
        return (float) core().nbtReal("height");
    }

    default void setInteractionHeight(float height) {
        core().nbtSet("height", "float", (double) height);
    }

    default boolean isResponsive() {
        return core().nbtFlag("response");
    }

    default void setResponsive(boolean responsive) {
        core().nbtSet("response", "bool", responsive);
    }
}
