package org.basaltmc.bukkit;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.minimessage.MiniMessage;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;
import net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer;

public final class Text {
    private static final LegacyComponentSerializer LEGACY = LegacyComponentSerializer.legacySection();

    private Text() {}

    public static String mini(Component component) {
        return MiniMessage.miniMessage().serialize(component);
    }

    public static String mini(String legacy) {
        if (legacy == null) {
            return "";
        }
        return mini(LEGACY.deserialize(legacy));
    }

    public static String plain(Component component) {
        return PlainTextComponentSerializer.plainText().serialize(component);
    }

    public static String legacy(Component component) {
        return LEGACY.serialize(component);
    }
}
