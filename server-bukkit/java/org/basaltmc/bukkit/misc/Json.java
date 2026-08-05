package org.basaltmc.bukkit.misc;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.gson.GsonComponentSerializer;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;

public final class Json {
    private static final GsonComponentSerializer kGson = GsonComponentSerializer.gson();
    private static final LegacyComponentSerializer kLegacy = LegacyComponentSerializer.legacySection();

    private Json() {}

    public static String of(Component component) {
        return kGson.serialize(component == null ? Component.empty() : component);
    }

    public static Component component(String json) {
        if (json == null || json.isEmpty()) {
            return Component.empty();
        }
        try {
            return kGson.deserialize(json);
        } catch (RuntimeException e) {
            return Component.text(json);
        }
    }

    public static String legacyOf(Component component) {
        return kLegacy.serialize(component == null ? Component.empty() : component);
    }

    public static Component fromLegacy(String text) {
        return text == null ? Component.empty() : kLegacy.deserialize(text);
    }
}
