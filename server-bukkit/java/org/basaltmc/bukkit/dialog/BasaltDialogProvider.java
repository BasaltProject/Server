package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.Dialog;
import io.papermc.paper.registry.data.dialog.ActionButton;
import io.papermc.paper.registry.data.dialog.DialogBase;
import io.papermc.paper.registry.data.dialog.DialogInstancesProvider;
import io.papermc.paper.registry.data.dialog.action.DialogAction;
import io.papermc.paper.registry.data.dialog.action.DialogActionCallback;
import io.papermc.paper.registry.data.dialog.body.ItemDialogBody;
import io.papermc.paper.registry.data.dialog.body.PlainMessageDialogBody;
import io.papermc.paper.registry.data.dialog.input.BooleanDialogInput;
import io.papermc.paper.registry.data.dialog.input.NumberRangeDialogInput;
import io.papermc.paper.registry.data.dialog.input.SingleOptionDialogInput;
import io.papermc.paper.registry.data.dialog.input.TextDialogInput;
import io.papermc.paper.registry.data.dialog.type.ConfirmationType;
import io.papermc.paper.registry.data.dialog.type.DialogListType;
import io.papermc.paper.registry.data.dialog.type.MultiActionType;
import io.papermc.paper.registry.data.dialog.type.NoticeType;
import io.papermc.paper.registry.data.dialog.type.ServerLinksType;
import io.papermc.paper.registry.set.RegistrySet;
import java.util.List;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.nbt.api.BinaryTagHolder;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.event.ClickCallback;
import net.kyori.adventure.text.event.ClickEvent;
import org.bukkit.inventory.ItemStack;
import org.jspecify.annotations.Nullable;

public final class BasaltDialogProvider implements DialogInstancesProvider {
    @Override
    public DialogBase.Builder dialogBaseBuilder(Component title) {
        return new DialogParts.BaseBuilder(title);
    }

    @Override
    public ActionButton.Builder actionButtonBuilder(Component label) {
        return new DialogParts.ButtonBuilder(label);
    }

    @Override
    public DialogAction.CustomClickAction register(DialogActionCallback callback, ClickCallback.Options options) {
        return DialogCallbacks.register(callback, options);
    }

    @Override
    public DialogAction.StaticAction staticAction(ClickEvent value) {
        return new DialogParts.Static(java.util.Objects.requireNonNull(value, "value"));
    }

    @Override
    public DialogAction.CommandTemplateAction commandTemplate(String template) {
        return new DialogParts.Template(java.util.Objects.requireNonNull(template, "template"));
    }

    @Override
    public DialogAction.CustomClickAction customClick(Key id, @Nullable BinaryTagHolder additions) {
        return new DialogParts.Custom(java.util.Objects.requireNonNull(id, "id"), additions);
    }

    @Override
    public ItemDialogBody.Builder itemDialogBodyBuilder(ItemStack itemStack) {
        return new DialogParts.ItemBodyBuilder(itemStack);
    }

    @Override
    public PlainMessageDialogBody plainMessageDialogBody(Component component) {
        return plainMessageDialogBody(component, 200);
    }

    @Override
    public PlainMessageDialogBody plainMessageDialogBody(Component component, int width) {
        DialogParts.checkWidth(width, 1024, "width");
        return new DialogParts.Plain(java.util.Objects.requireNonNull(component, "component"), width);
    }

    @Override
    public BooleanDialogInput.Builder booleanBuilder(String key, Component label) {
        return new DialogInputs.BoolBuilder(key, label);
    }

    @Override
    public NumberRangeDialogInput.Builder numberRangeBuilder(String key, Component label, float start, float end) {
        return new DialogInputs.RangeBuilder(key, label, start, end);
    }

    @Override
    public SingleOptionDialogInput.Builder singleOptionBuilder(String key, Component label,
            List<SingleOptionDialogInput.OptionEntry> entries) {
        return new DialogInputs.OptionBuilder(key, label, entries);
    }

    @Override
    public SingleOptionDialogInput.OptionEntry singleOptionEntry(String id, @Nullable Component display,
            boolean initial) {
        return new DialogInputs.Entry(java.util.Objects.requireNonNull(id, "id"), display, initial);
    }

    @Override
    public TextDialogInput.Builder textBuilder(String key, Component label) {
        return new DialogInputs.TextBuilder(key, label);
    }

    @Override
    public TextDialogInput.MultilineOptions multilineOptions(@Nullable Integer maxLines, @Nullable Integer height) {
        if (maxLines != null && maxLines < 1) {
            throw new IllegalArgumentException("maxLines must be positive: " + maxLines);
        }
        if (height != null && (height < 1 || height > 512)) {
            throw new IllegalArgumentException("height must be between 1 and 512: " + height);
        }
        return new DialogInputs.Multiline(maxLines, height);
    }

    @Override
    public ConfirmationType confirmation(ActionButton yesButton, ActionButton noButton) {
        return new DialogTypes.Confirmation(java.util.Objects.requireNonNull(yesButton, "yesButton"),
            java.util.Objects.requireNonNull(noButton, "noButton"));
    }

    @Override
    public DialogListType.Builder dialogList(RegistrySet<Dialog> dialogs) {
        return new DialogTypes.ListBuilder(dialogs);
    }

    @Override
    public MultiActionType.Builder multiAction(List<ActionButton> actions) {
        return new DialogTypes.MultiBuilder(actions);
    }

    @Override
    public NoticeType notice() {
        return notice(ActionButton.builder(Component.translatable("gui.ok")).build());
    }

    @Override
    public NoticeType notice(ActionButton action) {
        return new DialogTypes.Notice(java.util.Objects.requireNonNull(action, "action"));
    }

    @Override
    public ServerLinksType serverLinks(@Nullable ActionButton exitAction, int columns, int buttonWidth) {
        return new DialogTypes.Links(exitAction, columns, buttonWidth);
    }
}
