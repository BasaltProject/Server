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

    default PreviousInteraction getLastAttack() {
        return previous("attack");
    }

    default PreviousInteraction getLastInteraction() {
        return previous("interaction");
    }

    private PreviousInteraction previous(String key) {
        java.util.Map<String, Object> stored = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "").get(key));
        if (stored == null || !(stored.get("player") instanceof int[] player)
            || !(stored.get("timestamp") instanceof Number timestamp)) {
            return null;
        }
        java.util.UUID who = org.basaltmc.bukkit.item.Convert.uuidOf(player);
        long when = timestamp.longValue();
        return new PreviousInteraction() {
            @Override
            public org.bukkit.OfflinePlayer getPlayer() {
                return org.bukkit.Bukkit.getOfflinePlayer(who);
            }

            @Override
            public long getTimestamp() {
                return when;
            }
        };
    }
}
