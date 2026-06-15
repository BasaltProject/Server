package org.basaltmc.bukkit.entity.ops;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.minimessage.MiniMessage;
import org.basaltmc.bukkit.Text;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.TextDisplay;

public interface TextDisplayOps extends TextDisplay, Cored {
    default Component text() {
        String stored = core().text("display_text");
        return stored == null ? Component.empty() : MiniMessage.miniMessage().deserialize(stored);
    }

    default void text(Component text) {
        core().set("display_text", text == null ? null : Text.mini(text));
    }

    default String getText() {
        return Text.legacy(text());
    }

    default void setText(String text) {
        core().set("display_text", text == null ? null : Text.mini(text));
    }
}
