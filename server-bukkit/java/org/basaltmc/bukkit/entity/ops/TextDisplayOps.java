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

    default int getLineWidth() {
        return core().nbtInt("line_width", 200);
    }

    default void setLineWidth(int width) {
        core().nbtSet("line_width", "int", width);
    }

    default org.bukkit.Color getBackgroundColor() {
        int color = core().nbtInt("background", 1073741824);
        return color == 1073741824 ? null : org.bukkit.Color.fromARGB(color);
    }

    default void setBackgroundColor(org.bukkit.Color color) {
        core().nbtSet("background", "int", color == null ? 1073741824 : color.asARGB());
    }

    default byte getTextOpacity() {
        return (byte) core().nbtInt("text_opacity", -1);
    }

    default void setTextOpacity(byte opacity) {
        core().nbtSet("text_opacity", "byte", opacity);
    }

    default boolean isShadowed() {
        return core().nbtFlag("shadow");
    }

    default void setShadowed(boolean shadow) {
        core().nbtSet("shadow", "bool", shadow);
    }

    default boolean isSeeThrough() {
        return core().nbtFlag("see_through");
    }

    default void setSeeThrough(boolean seeThrough) {
        core().nbtSet("see_through", "bool", seeThrough);
    }

    default boolean isDefaultBackground() {
        return core().nbtFlag("default_background");
    }

    default void setDefaultBackground(boolean defaultBackground) {
        core().nbtSet("default_background", "bool", defaultBackground);
    }

    default TextDisplay.TextAlignment getAlignment() {
        String name = core().nbtText("alignment");
        return name == null ? TextDisplay.TextAlignment.CENTER
            : TextDisplay.TextAlignment.valueOf(name.toUpperCase(java.util.Locale.ROOT));
    }

    default void setAlignment(TextDisplay.TextAlignment alignment) {
        if (alignment == null) {
            throw new IllegalArgumentException("Alignment cannot be null");
        }
        core().nbtSet("alignment", "string", alignment.name().toLowerCase(java.util.Locale.ROOT));
    }
}
