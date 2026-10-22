package net.minecraft.network.chat;

public interface Component extends FormattedText {
    String getString();

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
