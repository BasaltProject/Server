package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.Dialog;
import io.papermc.paper.registry.data.dialog.ActionButton;
import io.papermc.paper.registry.data.dialog.type.ConfirmationType;
import io.papermc.paper.registry.data.dialog.type.DialogListType;
import io.papermc.paper.registry.data.dialog.type.MultiActionType;
import io.papermc.paper.registry.data.dialog.type.NoticeType;
import io.papermc.paper.registry.data.dialog.type.ServerLinksType;
import io.papermc.paper.registry.set.RegistrySet;
import java.util.List;
import org.jspecify.annotations.Nullable;

final class DialogTypes {
    private DialogTypes() {}

    private static void checkColumns(int columns) {
        if (columns < 1) {
            throw new IllegalArgumentException("columns must be positive: " + columns);
        }
    }

    record Notice(ActionButton action) implements NoticeType {}

    record Confirmation(ActionButton yesButton, ActionButton noButton) implements ConfirmationType {}

    record Links(@Nullable ActionButton exitAction, int columns, int buttonWidth) implements ServerLinksType {
        Links {
            checkColumns(columns);
            DialogParts.checkWidth(buttonWidth, 1024, "buttonWidth");
        }
    }

    record Multi(List<ActionButton> actions, @Nullable ActionButton exitAction, int columns)
            implements MultiActionType {}

    static final class MultiBuilder implements MultiActionType.Builder {
        private final List<ActionButton> actions;
        private ActionButton exitAction;
        private int columns = 2;

        MultiBuilder(List<ActionButton> actions) {
            if (actions.isEmpty()) {
                throw new IllegalArgumentException("actions cannot be empty");
            }
            this.actions = List.copyOf(actions);
        }

        @Override
        public MultiActionType.Builder exitAction(@Nullable ActionButton exitAction) {
            this.exitAction = exitAction;
            return this;
        }

        @Override
        public MultiActionType.Builder columns(int columns) {
            checkColumns(columns);
            this.columns = columns;
            return this;
        }

        @Override
        public MultiActionType build() {
            return new Multi(actions, exitAction, columns);
        }
    }

    record Listing(RegistrySet<Dialog> dialogs, @Nullable ActionButton exitAction, int columns, int buttonWidth)
            implements DialogListType {}

    static final class ListBuilder implements DialogListType.Builder {
        private final RegistrySet<Dialog> dialogs;
        private ActionButton exitAction;
        private int columns = 2;
        private int buttonWidth = 150;

        ListBuilder(RegistrySet<Dialog> dialogs) {
            this.dialogs = java.util.Objects.requireNonNull(dialogs, "dialogs");
        }

        @Override
        public DialogListType.Builder exitAction(@Nullable ActionButton exitAction) {
            this.exitAction = exitAction;
            return this;
        }

        @Override
        public DialogListType.Builder columns(int columns) {
            checkColumns(columns);
            this.columns = columns;
            return this;
        }

        @Override
        public DialogListType.Builder buttonWidth(int buttonWidth) {
            DialogParts.checkWidth(buttonWidth, 1024, "buttonWidth");
            this.buttonWidth = buttonWidth;
            return this;
        }

        @Override
        public DialogListType build() {
            return new Listing(dialogs, exitAction, columns, buttonWidth);
        }
    }
}
