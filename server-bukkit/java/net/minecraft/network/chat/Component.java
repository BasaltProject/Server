package net.minecraft.network.chat;

public interface Component extends FormattedText {
    String getString();

    default net.kyori.adventure.text.Component asAdventure() {
        return net.kyori.adventure.text.Component.text(getString());
    }

    static MutableComponent literal(String text) {
        return new MutableComponent(text, false);
    }

    static MutableComponent translatable(String key) {
        return new MutableComponent(key, true);
    }

    static Component empty() {
        return literal("");
    }
}
