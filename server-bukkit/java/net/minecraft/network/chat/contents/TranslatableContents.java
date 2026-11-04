// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;

import com.mojang.serialization.MapCodec;
import java.util.Arrays;
import java.util.Objects;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.network.chat.Style;
import org.basaltmc.bukkit.facade.ChatJson;

public class TranslatableContents implements ComponentContents {
    public static final Object[] NO_ARGS = new Object[0];
    public static final MapCodec<TranslatableContents> MAP_CODEC = ChatJson.translatableMapCodec();
    private final String key;
    private final String fallback;
    private final Object[] args;

    public static boolean isAllowedPrimitiveArgument(Object object) {
        return object instanceof Number || object instanceof Boolean || object instanceof String;
    }

    public TranslatableContents(String key, String fallback, Object[] args) {
        this.key = key;
        this.fallback = fallback;
        this.args = args;
    }

    @Override
    public MapCodec<TranslatableContents> codec() {
        return MAP_CODEC;
    }

    private String template() {
        return net.minecraft.locale.Language.getInstance().getOrDefault(key, fallback != null ? fallback : key);
    }

    private void format(java.util.function.BiConsumer<Style, String> literal,
        java.util.function.BiConsumer<Style, Component> component, Style style) {
        String template = template();
        StringBuilder pending = new StringBuilder();
        int next = 0;
        for (int i = 0; i < template.length(); i++) {
            char c = template.charAt(i);
            if (c != '%' || i + 1 >= template.length()) {
                pending.append(c);
                continue;
            }
            char kind = template.charAt(i + 1);
            if (kind == '%') {
                pending.append('%');
                i++;
                continue;
            }
            int end = i + 1;
            int index = -1;
            while (end < template.length() && Character.isDigit(template.charAt(end))) {
                end++;
            }
            if (end > i + 1 && end + 1 < template.length() && template.charAt(end) == '$' && template.charAt(end + 1) == 's') {
                index = Integer.parseInt(template.substring(i + 1, end)) - 1;
                end++;
            } else if (kind == 's') {
                index = next++;
                end = i + 1;
            } else {
                pending.append(c);
                continue;
            }
            if (pending.length() > 0) {
                literal.accept(style, pending.toString());
                pending.setLength(0);
            }
            if (index >= 0 && index < args.length) {
                Object arg = args[index];
                if (arg instanceof Component argComponent) {
                    component.accept(style, argComponent);
                } else {
                    literal.accept(style, String.valueOf(arg));
                }
            }
            i = end;
        }
        if (pending.length() > 0) {
            literal.accept(style, pending.toString());
        }
    }

    @Override
    public <T> Optional<T> visit(FormattedText.StyledContentConsumer<T> output, Style currentStyle) {
        java.util.List<java.util.function.Supplier<Optional<T>>> steps = new java.util.ArrayList<>();
        format((style, text) -> steps.add(() -> output.accept(style, text)),
            (style, component) -> steps.add(() -> component.visit(output, style)), currentStyle);
        for (java.util.function.Supplier<Optional<T>> step : steps) {
            Optional<T> result = step.get();
            if (result.isPresent()) {
                return result;
            }
        }
        return Optional.empty();
    }

    @Override
    public <T> Optional<T> visit(FormattedText.ContentConsumer<T> output) {
        return visit((style, text) -> output.accept(text), Style.EMPTY);
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof TranslatableContents other && Objects.equals(key, other.key)
            && Objects.equals(fallback, other.fallback) && Arrays.equals(args, other.args);
    }

    @Override
    public int hashCode() {
        return 31 * (31 * Objects.hashCode(key) + Objects.hashCode(fallback)) + Arrays.hashCode(args);
    }

    @Override
    public String toString() {
        return "translation{key='" + key + "'" + (fallback != null ? ", fallback='" + fallback + "'" : "")
            + ", args=" + Arrays.toString(args) + "}";
    }

    public String getKey() {
        return key;
    }

    public String getFallback() {
        return fallback;
    }

    public Object[] getArgs() {
        return args;
    }
}
