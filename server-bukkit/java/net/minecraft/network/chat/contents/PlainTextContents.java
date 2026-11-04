// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.network.chat.Style;

public interface PlainTextContents extends ComponentContents {
    MapCodec<PlainTextContents> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
        Codec.STRING.fieldOf("text").forGetter(PlainTextContents::text)).apply(instance, PlainTextContents::create));
    PlainTextContents EMPTY = new LiteralContents("");

    static PlainTextContents create(String text) {
        return text.isEmpty() ? EMPTY : new LiteralContents(text);
    }

    String text();

    @Override
    default MapCodec<PlainTextContents> codec() {
        return MAP_CODEC;
    }

    record LiteralContents(String text) implements PlainTextContents {
        @Override
        public <T> Optional<T> visit(FormattedText.ContentConsumer<T> output) {
            return output.accept(text);
        }

        @Override
        public <T> Optional<T> visit(FormattedText.StyledContentConsumer<T> output, Style currentStyle) {
            return output.accept(currentStyle, text);
        }

        @Override
        public String toString() {
            return "literal{" + text + "}";
        }
    }
}
