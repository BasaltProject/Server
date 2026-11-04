// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import it.unimi.dsi.fastutil.ints.Int2IntFunction;
import java.util.List;
import net.minecraft.network.chat.Style;
@FunctionalInterface
public interface FormattedCharSequence {
    FormattedCharSequence EMPTY = sink -> true;
    boolean accept(FormattedCharSink output);
    static FormattedCharSequence codepoint(int codepoint, Style style) {
        return sink -> sink.accept(0, style, codepoint);
    }
    static FormattedCharSequence forward(String plainText, Style style) {
        return plainText.isEmpty() ? EMPTY : sink -> StringDecomposer.iterate(plainText, style, sink);
    }
    static FormattedCharSequence forward(String plainText, Style style, Int2IntFunction modifier) {
        return plainText.isEmpty() ? EMPTY : sink -> StringDecomposer.iterate(plainText, style, decorateOutput(sink, modifier));
    }
    static FormattedCharSequence backward(String plainText, Style style) {
        return plainText.isEmpty() ? EMPTY : sink -> StringDecomposer.iterateBackwards(plainText, style, sink);
    }
    static FormattedCharSequence backward(String plainText, Style style, Int2IntFunction modifier) {
        return plainText.isEmpty() ? EMPTY : sink -> StringDecomposer.iterateBackwards(plainText, style, decorateOutput(sink, modifier));
    }
    static FormattedCharSink decorateOutput(FormattedCharSink output, Int2IntFunction modifier) {
        return (position, style, codepoint) -> output.accept(position, style, modifier.apply(Integer.valueOf(codepoint)));
    }
    static FormattedCharSequence composite() {
        return EMPTY;
    }
    static FormattedCharSequence composite(FormattedCharSequence part) {
        return part;
    }
    static FormattedCharSequence composite(FormattedCharSequence first, FormattedCharSequence second) {
        return fromPair(first, second);
    }
    static FormattedCharSequence composite(FormattedCharSequence... parts) {
        return fromList(List.of(parts));
    }
    static FormattedCharSequence composite(List<FormattedCharSequence> parts) {
        return switch (parts.size()) {
            case 0 -> EMPTY;
            case 1 -> parts.get(0);
            case 2 -> fromPair(parts.get(0), parts.get(1));
            default -> fromList(List.copyOf(parts));
        };
    }
    static FormattedCharSequence fromPair(FormattedCharSequence first, FormattedCharSequence second) {
        return sink -> first.accept(sink) && second.accept(sink);
    }
    static FormattedCharSequence fromList(List<FormattedCharSequence> partCopy) {
        return sink -> {
            for (FormattedCharSequence part : partCopy) {
                if (!part.accept(sink)) {
                    return false;
                }
            }
            return true;
        };
    }
}
