// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import net.minecraft.network.chat.FilterMask;

public record FilteredText(String raw, FilterMask mask) {
    public static final FilteredText EMPTY = passThrough("");

    public static FilteredText passThrough(String message) {
        return new FilteredText(message, FilterMask.PASS_THROUGH);
    }

    public static FilteredText fullyFiltered(String message) {
        return new FilteredText(message, FilterMask.FULLY_FILTERED);
    }

    public String filtered() {
        return mask.apply(raw);
    }

    public String filteredOrEmpty() {
        String shown = filtered();
        return shown == null ? "" : shown;
    }

    public boolean isFiltered() {
        return !mask.isEmpty();
    }
}
