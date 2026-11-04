// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import it.unimi.dsi.fastutil.ints.Int2IntFunction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.function.UnaryOperator;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.util.StringDecomposer;
public class SubStringSource {
    private final String plainText;
    private final List<Style> charStyles;
    private final Int2IntFunction reverseCharModifier;
    private SubStringSource(String plainText, List<Style> charStyles, Int2IntFunction reverseCharModifier) {
        this.plainText = plainText;
        this.charStyles = List.copyOf(charStyles);
        this.reverseCharModifier = reverseCharModifier;
    }
    public String getPlainText() {
        return plainText;
    }
    public List<FormattedCharSequence> substring(int start, int length, boolean reverse) {
        if (length == 0) {
            return List.of();
        }
        List<FormattedCharSequence> parts = new ArrayList<>();
        Style runStyle = charStyles.get(start);
        int runStart = start;
        for (int i = 1; i < length; i++) {
            Style style = charStyles.get(start + i);
            if (!style.equals(runStyle)) {
                parts.add(run(plainText.substring(runStart, start + i), runStyle, reverse));
                runStyle = style;
                runStart = start + i;
            }
        }
        if (runStart < start + length) {
            parts.add(run(plainText.substring(runStart, start + length), runStyle, reverse));
        }
        return reverse ? Collections.unmodifiableList(reverseOf(parts)) : parts;
    }
    private FormattedCharSequence run(String text, Style style, boolean reverse) {
        return reverse ? FormattedCharSequence.backward(text, style, reverseCharModifier) : FormattedCharSequence.forward(text, style);
    }
    private static <T> List<T> reverseOf(List<T> list) {
        List<T> copy = new ArrayList<>(list);
        Collections.reverse(copy);
        return copy;
    }
    public static SubStringSource create(FormattedText text) {
        return create(text, ch -> ch, s -> s);
    }
    public static SubStringSource create(FormattedText text, Int2IntFunction reverseCharModifier, UnaryOperator<String> shaper) {
        StringBuilder plain = new StringBuilder();
        List<Style> styles = new ArrayList<>();
        text.visit((style, contents) -> {
            StringDecomposer.iterateFormatted(contents, style, (position, charStyle, codepoint) -> {
                plain.appendCodePoint(codepoint);
                for (int i = 0; i < Character.charCount(codepoint); i++) {
                    styles.add(charStyle);
                }
                return true;
            });
            return Optional.empty();
        }, Style.EMPTY);
        return new SubStringSource(shaper.apply(plain.toString()), styles, reverseCharModifier);
    }
}
