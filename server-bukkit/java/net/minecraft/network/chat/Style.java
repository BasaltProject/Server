// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Objects;
import net.minecraft.ChatFormatting;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import org.basaltmc.bukkit.facade.ChatJson;

public final class Style {
    public static final Style EMPTY = new Style(null, null, null, null, null, null, null, null, null, null, null);
    public static final int NO_SHADOW = 0;
    private final TextColor color;
    private final Integer shadowColor;
    private final Boolean bold;
    private final Boolean italic;
    private final Boolean underlined;
    private final Boolean strikethrough;
    private final Boolean obfuscated;
    private final ClickEvent clickEvent;
    private final HoverEvent hoverEvent;
    private final String insertion;
    private final FontDescription font;

    private Style(TextColor color, Integer shadowColor, Boolean bold, Boolean italic, Boolean underlined,
        Boolean strikethrough, Boolean obfuscated, ClickEvent clickEvent, HoverEvent hoverEvent, String insertion,
        FontDescription font) {
        this.color = color;
        this.shadowColor = shadowColor;
        this.bold = bold;
        this.italic = italic;
        this.underlined = underlined;
        this.strikethrough = strikethrough;
        this.obfuscated = obfuscated;
        this.clickEvent = clickEvent;
        this.hoverEvent = hoverEvent;
        this.insertion = insertion;
        this.font = font;
    }

    private static Style create(Style s) {
        return s;
    }

    public TextColor getColor() {
        return color;
    }

    public Integer getShadowColor() {
        return shadowColor;
    }

    public boolean isBold() {
        return bold == Boolean.TRUE;
    }

    public boolean isItalic() {
        return italic == Boolean.TRUE;
    }

    public boolean isStrikethrough() {
        return strikethrough == Boolean.TRUE;
    }

    public boolean isUnderlined() {
        return underlined == Boolean.TRUE;
    }

    public boolean isObfuscated() {
        return obfuscated == Boolean.TRUE;
    }

    public boolean isEmpty() {
        return this == EMPTY;
    }

    public ClickEvent getClickEvent() {
        return clickEvent;
    }

    public HoverEvent getHoverEvent() {
        return hoverEvent;
    }

    public String getInsertion() {
        return insertion;
    }

    public FontDescription getFont() {
        return font;
    }

    private Style with(TextColor color, Integer shadow, Boolean bold, Boolean italic, Boolean underlined,
        Boolean strikethrough, Boolean obfuscated, ClickEvent click, HoverEvent hover, String insertion,
        FontDescription font) {
        Style style = new Style(color, shadow, bold, italic, underlined, strikethrough, obfuscated, click, hover,
            insertion, font);
        return style.equals(EMPTY) ? EMPTY : style;
    }

    public Style withColor(TextColor color) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withColor(ChatFormatting color) {
        return withColor(TextColor.fromLegacyFormat(color));
    }

    public Style withColor(int color) {
        return withColor(TextColor.fromRgb(color));
    }

    public Style withShadowColor(int shadowColor) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withoutShadow() {
        return withShadowColor(NO_SHADOW);
    }

    public Style withBold(Boolean bold) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withItalic(Boolean italic) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withUnderlined(Boolean underlined) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withStrikethrough(Boolean strikethrough) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withObfuscated(Boolean obfuscated) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withClickEvent(ClickEvent clickEvent) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withHoverEvent(HoverEvent hoverEvent) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withInsertion(String insertion) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style withFont(FontDescription font) {
        return with(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent, hoverEvent,
            insertion, font);
    }

    public Style applyFormat(ChatFormatting format) {
        TextColor newColor = color;
        Boolean newBold = bold;
        Boolean newItalic = italic;
        Boolean newStrikethrough = strikethrough;
        Boolean newUnderlined = underlined;
        Boolean newObfuscated = obfuscated;
        switch (format) {
            case OBFUSCATED -> newObfuscated = true;
            case BOLD -> newBold = true;
            case STRIKETHROUGH -> newStrikethrough = true;
            case UNDERLINE -> newUnderlined = true;
            case ITALIC -> newItalic = true;
            case RESET -> {
                return EMPTY;
            }
            default -> newColor = TextColor.fromLegacyFormat(format);
        }
        return with(newColor, shadowColor, newBold, newItalic, newUnderlined, newStrikethrough, newObfuscated,
            clickEvent, hoverEvent, insertion, font);
    }

    public Style applyLegacyFormat(ChatFormatting format) {
        TextColor newColor = color;
        Boolean newBold = bold;
        Boolean newItalic = italic;
        Boolean newStrikethrough = strikethrough;
        Boolean newUnderlined = underlined;
        Boolean newObfuscated = obfuscated;
        switch (format) {
            case OBFUSCATED -> newObfuscated = true;
            case BOLD -> newBold = true;
            case STRIKETHROUGH -> newStrikethrough = true;
            case UNDERLINE -> newUnderlined = true;
            case ITALIC -> newItalic = true;
            case RESET -> {
                return EMPTY;
            }
            default -> {
                newObfuscated = false;
                newBold = false;
                newStrikethrough = false;
                newUnderlined = false;
                newItalic = false;
                newColor = TextColor.fromLegacyFormat(format);
            }
        }
        return with(newColor, shadowColor, newBold, newItalic, newUnderlined, newStrikethrough, newObfuscated,
            clickEvent, hoverEvent, insertion, font);
    }

    public Style applyFormats(ChatFormatting... formats) {
        TextColor newColor = color;
        Boolean newBold = bold;
        Boolean newItalic = italic;
        Boolean newStrikethrough = strikethrough;
        Boolean newUnderlined = underlined;
        Boolean newObfuscated = obfuscated;
        for (ChatFormatting format : formats) {
            switch (format) {
                case OBFUSCATED -> newObfuscated = true;
                case BOLD -> newBold = true;
                case STRIKETHROUGH -> newStrikethrough = true;
                case UNDERLINE -> newUnderlined = true;
                case ITALIC -> newItalic = true;
                case RESET -> {
                    return EMPTY;
                }
                default -> newColor = TextColor.fromLegacyFormat(format);
            }
        }
        return with(newColor, shadowColor, newBold, newItalic, newUnderlined, newStrikethrough, newObfuscated,
            clickEvent, hoverEvent, insertion, font);
    }

    public Style applyTo(Style other) {
        if (this == EMPTY) {
            return other;
        }
        if (other == EMPTY) {
            return this;
        }
        return with(color != null ? color : other.color, shadowColor != null ? shadowColor : other.shadowColor,
            bold != null ? bold : other.bold, italic != null ? italic : other.italic,
            underlined != null ? underlined : other.underlined,
            strikethrough != null ? strikethrough : other.strikethrough,
            obfuscated != null ? obfuscated : other.obfuscated, clickEvent != null ? clickEvent : other.clickEvent,
            hoverEvent != null ? hoverEvent : other.hoverEvent, insertion != null ? insertion : other.insertion,
            font != null ? font : other.font);
    }

    @Override
    public String toString() {
        StringBuilder text = new StringBuilder("{");
        append(text, "color", color);
        append(text, "shadowColor", shadowColor);
        append(text, "bold", bold);
        append(text, "italic", italic);
        append(text, "underlined", underlined);
        append(text, "strikethrough", strikethrough);
        append(text, "obfuscated", obfuscated);
        append(text, "clickEvent", clickEvent);
        append(text, "hoverEvent", hoverEvent);
        append(text, "insertion", insertion);
        append(text, "font", font);
        return text.append('}').toString();
    }

    private static void append(StringBuilder text, String name, Object value) {
        if (value != null) {
            if (text.length() > 1) {
                text.append(',');
            }
            text.append(name).append('=').append(value);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        return o instanceof Style other && Objects.equals(color, other.color)
            && Objects.equals(shadowColor, other.shadowColor) && Objects.equals(bold, other.bold)
            && Objects.equals(italic, other.italic) && Objects.equals(underlined, other.underlined)
            && Objects.equals(strikethrough, other.strikethrough) && Objects.equals(obfuscated, other.obfuscated)
            && Objects.equals(clickEvent, other.clickEvent) && Objects.equals(hoverEvent, other.hoverEvent)
            && Objects.equals(insertion, other.insertion) && Objects.equals(font, other.font);
    }

    @Override
    public int hashCode() {
        return Objects.hash(color, shadowColor, bold, italic, underlined, strikethrough, obfuscated, clickEvent,
            hoverEvent, insertion);
    }

    public static class Serializer {
        public static final MapCodec<Style> MAP_CODEC = ChatJson.styleMapCodec();
        public static final Codec<Style> CODEC = MAP_CODEC.codec();
        public static final StreamCodec<RegistryFriendlyByteBuf, Style> TRUSTED_STREAM_CODEC =
            ChatJson.nbtStream(CODEC);
    }
}
