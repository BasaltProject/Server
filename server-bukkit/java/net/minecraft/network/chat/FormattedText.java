// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import java.util.List;
import java.util.Optional;
import net.minecraft.util.Unit;

public interface FormattedText {
    Optional<Unit> STOP_ITERATION = Optional.of(Unit.INSTANCE);
    FormattedText EMPTY = new FormattedText() {
        @Override
        public <T> Optional<T> visit(ContentConsumer<T> output) {
            return Optional.empty();
        }

        @Override
        public <T> Optional<T> visit(StyledContentConsumer<T> output, Style parentStyle) {
            return Optional.empty();
        }
    };

    <T> Optional<T> visit(ContentConsumer<T> output);

    <T> Optional<T> visit(StyledContentConsumer<T> output, Style parentStyle);

    static FormattedText of(String text) {
        return new FormattedText() {
            @Override
            public <T> Optional<T> visit(ContentConsumer<T> output) {
                return output.accept(text);
            }

            @Override
            public <T> Optional<T> visit(StyledContentConsumer<T> output, Style parentStyle) {
                return output.accept(parentStyle, text);
            }
        };
    }

    static FormattedText of(String text, Style style) {
        return new FormattedText() {
            @Override
            public <T> Optional<T> visit(ContentConsumer<T> output) {
                return output.accept(text);
            }

            @Override
            public <T> Optional<T> visit(StyledContentConsumer<T> output, Style parentStyle) {
                return output.accept(style.applyTo(parentStyle), text);
            }
        };
    }

    static FormattedText composite(FormattedText... parts) {
        return composite(List.of(parts));
    }

    static FormattedText composite(List<? extends FormattedText> parts) {
        return new FormattedText() {
            @Override
            public <T> Optional<T> visit(ContentConsumer<T> output) {
                for (FormattedText part : parts) {
                    Optional<T> result = part.visit(output);
                    if (result.isPresent()) {
                        return result;
                    }
                }
                return Optional.empty();
            }

            @Override
            public <T> Optional<T> visit(StyledContentConsumer<T> output, Style parentStyle) {
                for (FormattedText part : parts) {
                    Optional<T> result = part.visit(output, parentStyle);
                    if (result.isPresent()) {
                        return result;
                    }
                }
                return Optional.empty();
            }
        };
    }

    default String getString() {
        StringBuilder builder = new StringBuilder();
        visit(content -> {
            builder.append(content);
            return Optional.empty();
        });
        return builder.toString();
    }

    interface ContentConsumer<T> {
        Optional<T> accept(String contents);
    }

    interface StyledContentConsumer<T> {
        Optional<T> accept(Style style, String contents);
    }
}
