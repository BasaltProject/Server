// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.List;
import com.mojang.brigadier.Message;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import java.util.Optional;
import java.util.function.Function;
public class ComponentUtils {
    public static final String DEFAULT_SEPARATOR_TEXT = ", ";
    public static final Component DEFAULT_SEPARATOR = Component.literal(", ").withStyle(net.minecraft.ChatFormatting.GRAY);
    public static final Component DEFAULT_NO_STYLE_SEPARATOR = Component.literal(", ");
    public static MutableComponent mergeStyles(MutableComponent component, Style style) {
        if (style.isEmpty()) {
            return component;
        }
        Style inner = component.getStyle();
        if (inner.isEmpty()) {
            return component.setStyle(style);
        }
        return inner.equals(style) ? component : component.setStyle(inner.applyTo(style));
    }
    public static Component mergeStyles(Component component, Style style) {
        if (style.isEmpty()) {
            return component;
        }
        Style inner = component.getStyle();
        if (inner.isEmpty()) {
            return component.copy().setStyle(style);
        }
        return inner.equals(style) ? component : component.copy().setStyle(inner.applyTo(style));
    }
    public static Optional<MutableComponent> resolve(ResolutionContext context, Optional<Component> component, int recursionDepth) throws CommandSyntaxException {
        return component.isPresent() ? Optional.of(resolve(context, component.get(), recursionDepth)) : Optional.empty();
    }
    public static MutableComponent resolve(ResolutionContext context, Component component) throws CommandSyntaxException {
        return resolve(context, component, 0);
    }
    public static MutableComponent resolve(ResolutionContext context, Component component, int recursionDepth) throws CommandSyntaxException {
        final MutableComponent result = component.getContents().resolve(context, recursionDepth + 1);
        for (Component sibling : component.getSiblings()) {
            result.append(resolve(context, sibling, recursionDepth + 1));
        }
        return result.withStyle(component.getStyle());
    }
    public static Component formatList(Collection<String> values) {
        return formatAndSortList(values, value -> Component.literal(value).withStyle(net.minecraft.ChatFormatting.GREEN));
    }
    public static <T extends Comparable<T>> Component formatAndSortList(Collection<T> values, Function<T, Component> formatter) {
        if (values.isEmpty()) {
            return CommonComponents.EMPTY;
        }
        if (values.size() == 1) {
            return formatter.apply(values.iterator().next());
        }
        List<T> sorted = new java.util.ArrayList<>(values);
        sorted.sort(Comparable::compareTo);
        return formatList(sorted, formatter);
    }
    public static <T> Component formatList(Collection<? extends T> values, Function<T, Component> formatter) {
        return formatList(values, DEFAULT_SEPARATOR, formatter);
    }
    public static <T> MutableComponent formatList(Collection<? extends T> values, Optional<? extends Component> separator, Function<T, Component> formatter) {
        return formatList(values, separator.<Component>map(c -> c).orElse(DEFAULT_SEPARATOR), formatter);
    }
    public static Component formatList(Collection<? extends Component> values, Component separator) {
        return formatList(values, separator, Function.identity());
    }
    public static <T> MutableComponent formatList(Collection<? extends T> values, Component separator, Function<T, Component> formatter) {
        if (values.isEmpty()) {
            return Component.empty();
        }
        if (values.size() == 1) {
            return formatter.apply(values.iterator().next()).copy();
        }
        final MutableComponent result = Component.empty();
        boolean first = true;
        for (T value : values) {
            if (!first) {
        result.append(separator);
            }
            result.append(formatter.apply(value));
            first = false;
        }
        return result;
    }
    public static MutableComponent wrapInSquareBrackets(Component inner) {
        return Component.translatable("chat.square_brackets", inner);
    }
    public static Component fromMessage(Message message) {
        return message instanceof Component component ? component : Component.literal(message.getString());
    }
    public static boolean isTranslationResolvable(Component component) {
        if (component != null && component.getContents() instanceof net.minecraft.network.chat.contents.TranslatableContents t) {
            return t.getFallback() != null || net.minecraft.locale.Language.getInstance().has(t.getKey());
        }
        return true;
    }
    public static MutableComponent copyOnClickText(String text) {
        return wrapInSquareBrackets(Component.literal(text).withStyle(style -> style.withColor(net.minecraft.ChatFormatting.GREEN)
            .withClickEvent(new ClickEvent.CopyToClipboard(text))
            .withHoverEvent(new HoverEvent.ShowText(Component.translatable("chat.copy.click"))).withInsertion(text)));
    }
}
