package org.basaltmc.bukkit.dialog;

import io.papermc.paper.registry.data.dialog.input.BooleanDialogInput;
import io.papermc.paper.registry.data.dialog.input.NumberRangeDialogInput;
import io.papermc.paper.registry.data.dialog.input.SingleOptionDialogInput;
import io.papermc.paper.registry.data.dialog.input.TextDialogInput;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.jspecify.annotations.Nullable;

/** The four kinds of input a dialog can ask for. */
final class DialogInputs {
    private DialogInputs() {}

    record Bool(String key, Component label, boolean initial, String onTrue, String onFalse)
            implements BooleanDialogInput {}

    static final class BoolBuilder implements BooleanDialogInput.Builder {
        private final String key;
        private final Component label;
        private boolean initial;
        private String onTrue = "true";
        private String onFalse = "false";

        BoolBuilder(String key, Component label) {
            this.key = java.util.Objects.requireNonNull(key, "key");
            this.label = java.util.Objects.requireNonNull(label, "label");
        }

        @Override
        public BooleanDialogInput.Builder initial(boolean initial) {
            this.initial = initial;
            return this;
        }

        @Override
        public BooleanDialogInput.Builder onTrue(String onTrue) {
            this.onTrue = java.util.Objects.requireNonNull(onTrue, "onTrue");
            return this;
        }

        @Override
        public BooleanDialogInput.Builder onFalse(String onFalse) {
            this.onFalse = java.util.Objects.requireNonNull(onFalse, "onFalse");
            return this;
        }

        @Override
        public BooleanDialogInput build() {
            return new Bool(key, label, initial, onTrue, onFalse);
        }
    }

    record Range(String key, int width, Component label, String labelFormat, float start, float end,
            @Nullable Float initial, @Nullable Float step) implements NumberRangeDialogInput {}

    static final class RangeBuilder implements NumberRangeDialogInput.Builder {
        private final String key;
        private final Component label;
        private final float start;
        private final float end;
        private int width = 200;
        private String labelFormat = "options.generic_value";
        private Float initial;
        private Float step;

        RangeBuilder(String key, Component label, float start, float end) {
            this.key = java.util.Objects.requireNonNull(key, "key");
            this.label = java.util.Objects.requireNonNull(label, "label");
            this.start = start;
            this.end = end;
        }

        @Override
        public NumberRangeDialogInput.Builder width(int width) {
            DialogParts.checkWidth(width, 1024, "width");
            this.width = width;
            return this;
        }

        @Override
        public NumberRangeDialogInput.Builder labelFormat(String labelFormat) {
            this.labelFormat = java.util.Objects.requireNonNull(labelFormat, "labelFormat");
            return this;
        }

        @Override
        public NumberRangeDialogInput.Builder initial(@Nullable Float initial) {
            this.initial = initial;
            return this;
        }

        @Override
        public NumberRangeDialogInput.Builder step(@Nullable Float step) {
            if (step != null && step <= 0.0F) {
                throw new IllegalArgumentException("step must be positive: " + step);
            }
            this.step = step;
            return this;
        }

        @Override
        public NumberRangeDialogInput build() {
            if (initial != null && (initial < Math.min(start, end) || initial > Math.max(start, end))) {
                throw new IllegalStateException("Initial value " + initial + " is outside of range [" + start + ", "
                    + end + "]");
            }
            return new Range(key, width, label, labelFormat, start, end, initial, step);
        }
    }

    record Entry(String id, @Nullable Component display, boolean initial)
            implements SingleOptionDialogInput.OptionEntry {}

    record Option(String key, int width, List<SingleOptionDialogInput.OptionEntry> entries, Component label,
            boolean labelVisible) implements SingleOptionDialogInput {}

    static final class OptionBuilder implements SingleOptionDialogInput.Builder {
        private final String key;
        private final Component label;
        private final List<SingleOptionDialogInput.OptionEntry> entries;
        private int width = 200;
        private boolean labelVisible = true;

        OptionBuilder(String key, Component label, List<SingleOptionDialogInput.OptionEntry> entries) {
            this.key = java.util.Objects.requireNonNull(key, "key");
            this.label = java.util.Objects.requireNonNull(label, "label");
            if (entries.isEmpty()) {
                throw new IllegalArgumentException("entries cannot be empty");
            }
            this.entries = List.copyOf(entries);
        }

        @Override
        public SingleOptionDialogInput.Builder width(int width) {
            DialogParts.checkWidth(width, 1024, "width");
            this.width = width;
            return this;
        }

        @Override
        public SingleOptionDialogInput.Builder labelVisible(boolean labelVisible) {
            this.labelVisible = labelVisible;
            return this;
        }

        @Override
        public SingleOptionDialogInput build() {
            if (entries.stream().filter(SingleOptionDialogInput.OptionEntry::initial).count() > 1) {
                throw new IllegalStateException("Multiple initial values");
            }
            return new Option(key, width, entries, label, labelVisible);
        }
    }

    record Multiline(@Nullable Integer maxLines, @Nullable Integer height) implements TextDialogInput.MultilineOptions {}

    record Text(String key, int width, Component label, boolean labelVisible, String initial, int maxLength,
            TextDialogInput.@Nullable MultilineOptions multiline) implements TextDialogInput {}

    static final class TextBuilder implements TextDialogInput.Builder {
        private final String key;
        private final Component label;
        private int width = 200;
        private boolean labelVisible = true;
        private String initial = "";
        private int maxLength = 32;
        private TextDialogInput.MultilineOptions multiline;

        TextBuilder(String key, Component label) {
            this.key = java.util.Objects.requireNonNull(key, "key");
            this.label = java.util.Objects.requireNonNull(label, "label");
        }

        @Override
        public TextDialogInput.Builder width(int width) {
            DialogParts.checkWidth(width, 1024, "width");
            this.width = width;
            return this;
        }

        @Override
        public TextDialogInput.Builder labelVisible(boolean labelVisible) {
            this.labelVisible = labelVisible;
            return this;
        }

        @Override
        public TextDialogInput.Builder initial(String initial) {
            this.initial = java.util.Objects.requireNonNull(initial, "initial");
            return this;
        }

        @Override
        public TextDialogInput.Builder maxLength(int maxLength) {
            if (maxLength < 1) {
                throw new IllegalArgumentException("maxLength must be positive: " + maxLength);
            }
            this.maxLength = maxLength;
            return this;
        }

        @Override
        public TextDialogInput.Builder multiline(TextDialogInput.@Nullable MultilineOptions multiline) {
            this.multiline = multiline;
            return this;
        }

        @Override
        public TextDialogInput build() {
            if (initial.length() > maxLength) {
                throw new IllegalStateException("Default text length exceeds allowed size");
            }
            return new Text(key, width, label, labelVisible, initial, maxLength, multiline);
        }
    }
}
