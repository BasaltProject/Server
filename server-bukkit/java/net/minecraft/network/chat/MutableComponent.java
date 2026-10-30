package net.minecraft.network.chat;

import java.util.ArrayList;
import java.util.List;

public class MutableComponent implements Component {
    private final String text;
    private final boolean translatable;
    private final List<Component> siblings = new ArrayList<>();

    protected MutableComponent(String text, boolean translatable) {
        this.text = text;
        this.translatable = translatable;
    }

    public MutableComponent append(Component component) {
        siblings.add(component);
        return this;
    }

    public boolean isTranslatable() {
        return translatable;
    }

    public String text() {
        return text;
    }

    public List<Component> siblings() {
        return siblings;
    }

    @Override
    public net.kyori.adventure.text.Component asAdventure() {
        net.kyori.adventure.text.Component result = translatable
            ? net.kyori.adventure.text.Component.translatable(text) : net.kyori.adventure.text.Component.text(text);
        for (Component sibling : siblings) result = result.append(sibling.asAdventure());
        return result;
    }

    @Override
    public String getString() {
        StringBuilder out = new StringBuilder(text);
        for (Component sibling : siblings) {
            out.append(sibling.getString());
        }
        return out.toString();
    }
}
