package org.basaltmc.bukkit.dialog;

import io.papermc.paper.registry.data.dialog.ActionButton;
import io.papermc.paper.registry.data.dialog.DialogBase;
import io.papermc.paper.registry.data.dialog.action.DialogAction;
import io.papermc.paper.registry.data.dialog.body.DialogBody;
import io.papermc.paper.registry.data.dialog.body.ItemDialogBody;
import io.papermc.paper.registry.data.dialog.body.PlainMessageDialogBody;
import io.papermc.paper.registry.data.dialog.input.DialogInput;
import java.util.List;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.nbt.api.BinaryTagHolder;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.event.ClickEvent;
import org.bukkit.inventory.ItemStack;
import org.jspecify.annotations.Nullable;

final class DialogParts {
    private DialogParts() {}

    static void checkWidth(int width, int max, String what) {
        if (width < 1 || width > max) {
            throw new IllegalArgumentException(what + " must be between 1 and " + max + ": " + width);
        }
    }

    record Base(Component title, @Nullable Component externalTitle, boolean canCloseWithEscape, boolean pause,
            DialogBase.DialogAfterAction afterAction, List<DialogBody> body, List<DialogInput> inputs)
            implements DialogBase {}

    static final class BaseBuilder implements DialogBase.Builder {
        private final Component title;
        private Component externalTitle;
        private boolean canCloseWithEscape = true;
        private boolean pause = true;
        private DialogBase.DialogAfterAction afterAction = DialogBase.DialogAfterAction.CLOSE;
        private List<DialogBody> body = List.of();
        private List<DialogInput> inputs = List.of();

        BaseBuilder(Component title) {
            this.title = java.util.Objects.requireNonNull(title, "title");
        }

        @Override
        public DialogBase.Builder externalTitle(@Nullable Component externalTitle) {
            this.externalTitle = externalTitle;
            return this;
        }

        @Override
        public DialogBase.Builder canCloseWithEscape(boolean canCloseWithEscape) {
            this.canCloseWithEscape = canCloseWithEscape;
            return this;
        }

        @Override
        public DialogBase.Builder pause(boolean pause) {
            this.pause = pause;
            return this;
        }

        @Override
        public DialogBase.Builder afterAction(DialogBase.DialogAfterAction afterAction) {
            this.afterAction = java.util.Objects.requireNonNull(afterAction, "afterAction");
            return this;
        }

        @Override
        public DialogBase.Builder body(List<? extends DialogBody> body) {
            this.body = List.copyOf(body);
            return this;
        }

        @Override
        public DialogBase.Builder inputs(List<? extends DialogInput> inputs) {
            this.inputs = List.copyOf(inputs);
            return this;
        }

        @Override
        public DialogBase build() {
            if (pause && afterAction == DialogBase.DialogAfterAction.NONE) {
                throw new IllegalStateException(
                    "Dialogs that pause the game must use after_action values that unpause it after user action");
            }
            return new Base(title, externalTitle, canCloseWithEscape, pause, afterAction, body, inputs);
        }
    }

    record Button(Component label, @Nullable Component tooltip, int width, @Nullable DialogAction action)
            implements ActionButton {}

    static final class ButtonBuilder implements ActionButton.Builder {
        private final Component label;
        private Component tooltip;
        private int width = 150;
        private DialogAction action;

        ButtonBuilder(Component label) {
            this.label = java.util.Objects.requireNonNull(label, "label");
        }

        @Override
        public ActionButton.Builder tooltip(@Nullable Component tooltip) {
            this.tooltip = tooltip;
            return this;
        }

        @Override
        public ActionButton.Builder width(int width) {
            checkWidth(width, 1024, "width");
            this.width = width;
            return this;
        }

        @Override
        public ActionButton.Builder action(@Nullable DialogAction action) {
            this.action = action;
            return this;
        }

        @Override
        public ActionButton build() {
            return new Button(label, tooltip, width, action);
        }
    }

    record Static(ClickEvent value) implements DialogAction.StaticAction {}

    record Template(String template) implements DialogAction.CommandTemplateAction {}

    record Custom(Key id, @Nullable BinaryTagHolder additions) implements DialogAction.CustomClickAction {}

    record Plain(Component contents, int width) implements PlainMessageDialogBody {}

    record ItemBody(ItemStack item, @Nullable PlainMessageDialogBody description, boolean showDecorations,
            boolean showTooltip, int width, int height) implements ItemDialogBody {}

    static final class ItemBodyBuilder implements ItemDialogBody.Builder {
        private final ItemStack item;
        private PlainMessageDialogBody description;
        private boolean showDecorations = true;
        private boolean showTooltip = true;
        private int width = 16;
        private int height = 16;

        ItemBodyBuilder(ItemStack item) {
            this.item = java.util.Objects.requireNonNull(item, "item").clone();
        }

        @Override
        public ItemDialogBody.Builder description(@Nullable PlainMessageDialogBody description) {
            this.description = description;
            return this;
        }

        @Override
        public ItemDialogBody.Builder showDecorations(boolean showDecorations) {
            this.showDecorations = showDecorations;
            return this;
        }

        @Override
        public ItemDialogBody.Builder showTooltip(boolean showTooltip) {
            this.showTooltip = showTooltip;
            return this;
        }

        @Override
        public ItemDialogBody.Builder width(int width) {
            checkWidth(width, 256, "width");
            this.width = width;
            return this;
        }

        @Override
        public ItemDialogBody.Builder height(int height) {
            checkWidth(height, 256, "height");
            this.height = height;
            return this;
        }

        @Override
        public ItemDialogBody build() {
            return new ItemBody(item, description, showDecorations, showTooltip, width, height);
        }
    }
}
