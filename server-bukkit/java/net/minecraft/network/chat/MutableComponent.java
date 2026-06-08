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
    public String getString() {
        StringBuilder out = new StringBuilder(text);
        for (Component sibling : siblings) {
            out.append(sibling.getString());
        }
        return out.toString();
    }
}
