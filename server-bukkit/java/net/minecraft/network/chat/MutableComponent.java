// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.UnaryOperator;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.contents.PlainTextContents;
import net.minecraft.network.chat.contents.TranslatableContents;
import net.minecraft.util.FormattedCharSequence;

public class MutableComponent implements Component {
    private final ComponentContents contents;
    private final List<Component> siblings;
    private Style style;

    MutableComponent(ComponentContents contents, List<Component> siblings, Style style) {
        this.contents = contents;
        this.siblings = siblings;
        this.style = style;
    }

    public static MutableComponent create(ComponentContents contents) {
        return new MutableComponent(contents, new ArrayList<>(), Style.EMPTY);
    }

    public boolean isTranslatable() {
        return contents instanceof TranslatableContents;
    }

    public String text() {
        if (contents instanceof PlainTextContents plain) {
            return plain.text();
        }
        return contents instanceof TranslatableContents translatable ? translatable.getKey() : "";
    }

    public List<Component> siblings() {
        return siblings;
    }

    @Override
    public ComponentContents getContents() {
        return contents;
    }

    @Override
    public List<Component> getSiblings() {
        return siblings;
    }

    public MutableComponent setStyle(Style style) {
        this.style = style;
        return this;
    }

    @Override
    public Style getStyle() {
        return style;
    }

    public MutableComponent append(String text) {
        return append(Component.literal(text));
    }

    public MutableComponent append(Component component) {
        siblings.add(component);
        return this;
    }

    public MutableComponent withStyle(UnaryOperator<Style> updater) {
        setStyle(updater.apply(getStyle()));
        return this;
    }

    public MutableComponent withStyle(Style patch) {
        setStyle(patch.applyTo(getStyle()));
        return this;
    }

    public MutableComponent withStyle(ChatFormatting... formats) {
        setStyle(getStyle().applyFormats(formats));
        return this;
    }

    public MutableComponent withStyle(ChatFormatting format) {
        setStyle(getStyle().applyFormat(format));
        return this;
    }

    public MutableComponent withColor(int color) {
        setStyle(getStyle().withColor(color));
        return this;
    }

    public MutableComponent withColor(TextColor color) {
        setStyle(getStyle().withColor(color));
        return this;
    }

    public MutableComponent withoutShadow() {
        setStyle(getStyle().withoutShadow());
        return this;
    }

    @Override
    public FormattedCharSequence getVisualOrderText() {
        return Component.super.getVisualOrderText();
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof MutableComponent other && contents.equals(other.contents)
            && style.equals(other.style) && siblings.equals(other.siblings);
    }

    @Override
    public int hashCode() {
        return Objects.hash(contents, style, siblings);
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder(contents.toString());
        boolean hasStyle = !style.isEmpty();
        boolean hasSiblings = !siblings.isEmpty();
        if (hasStyle || hasSiblings) {
            result.append('[');
            if (hasStyle) {
                result.append("style=").append(style);
            }
            if (hasStyle && hasSiblings) {
                result.append(", ");
            }
            if (hasSiblings) {
                result.append("siblings=").append(siblings);
            }
            result.append(']');
        }
        return result.toString();
    }
}
