package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.JavaOps;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.ClickEvent;
import net.minecraft.server.dialog.ActionButton;
import net.minecraft.server.dialog.CommonButtonData;
import net.minecraft.server.dialog.CommonDialogData;
import net.minecraft.server.dialog.ConfirmationDialog;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.server.dialog.DialogListDialog;
import net.minecraft.server.dialog.Input;
import net.minecraft.server.dialog.MultiActionDialog;
import net.minecraft.server.dialog.NoticeDialog;
import net.minecraft.server.dialog.ServerLinksDialog;
import net.minecraft.server.dialog.action.Action;
import net.minecraft.server.dialog.action.CommandTemplate;
import net.minecraft.server.dialog.action.CustomAll;
import net.minecraft.server.dialog.action.StaticAction;
import net.minecraft.server.dialog.body.DialogBody;
import net.minecraft.server.dialog.body.ItemBody;
import net.minecraft.server.dialog.body.PlainMessage;
import net.minecraft.server.dialog.input.BooleanInput;
import net.minecraft.server.dialog.input.InputControl;
import net.minecraft.server.dialog.input.NumberRangeInput;
import net.minecraft.server.dialog.input.SingleOptionInput;
import net.minecraft.server.dialog.input.TextInput;

/** Writes the dialog family in its wire layout: a flat map per dialog, with the shared parts folded into it. */
public final class DialogCodecs {
    public static final Codec<Dialog> DIRECT_CODEC = new Codec<>() {
        @Override
        public <T> DataResult<Pair<Dialog, T>> decode(DynamicOps<T> ops, T input) {
            return DataResult.error(() -> "Dialogs can only be written by the Basalt facade");
        }

        @Override
        public <T> DataResult<T> encode(Dialog dialog, DynamicOps<T> ops, T prefix) {
            try {
                return DataResult.success(new Dynamic<>(JavaOps.INSTANCE, dialog(dialog)).convert(ops).getValue());
            } catch (RuntimeException failure) {
                return DataResult.error(() -> "Cannot encode dialog: " + failure);
            }
        }
    };

    private DialogCodecs() {}

    private static Object component(Component value) {
        return ComponentSerialization.CODEC.encodeStart(JavaOps.INSTANCE, value).getOrThrow();
    }

    private static void optional(Map<String, Object> out, String key, Optional<?> value, java.util.function.Function<Object, Object> encoder) {
        value.ifPresent(present -> out.put(key, encoder.apply(present)));
    }

    private static Map<String, Object> dialog(Dialog value) {
        Map<String, Object> out = new LinkedHashMap<>();
        if (value instanceof NoticeDialog notice) {
            out.put("type", "minecraft:notice");
            common(out, notice.common());
            out.put("action", button(notice.action()));
        } else if (value instanceof ConfirmationDialog confirmation) {
            out.put("type", "minecraft:confirmation");
            common(out, confirmation.common());
            out.put("yes", button(confirmation.yesButton()));
            out.put("no", button(confirmation.noButton()));
        } else if (value instanceof MultiActionDialog multi) {
            out.put("type", "minecraft:multi_action");
            common(out, multi.common());
            List<Object> actions = new ArrayList<>();
            multi.actions().forEach(each -> actions.add(button(each)));
            out.put("actions", actions);
            optional(out, "exit_action", multi.exitAction(), each -> button((ActionButton) each));
            out.put("columns", multi.columns());
        } else if (value instanceof DialogListDialog list) {
            out.put("type", "minecraft:dialog_list");
            common(out, list.common());
            out.put("dialogs", dialogSet(list.dialogs()));
            optional(out, "exit_action", list.exitAction(), each -> button((ActionButton) each));
            out.put("columns", list.columns());
            out.put("button_width", list.buttonWidth());
        } else if (value instanceof ServerLinksDialog links) {
            out.put("type", "minecraft:server_links");
            common(out, links.common());
            optional(out, "exit_action", links.exitAction(), each -> button((ActionButton) each));
            out.put("columns", links.columns());
            out.put("button_width", links.buttonWidth());
        } else {
            throw new IllegalArgumentException("Unknown dialog " + value);
        }
        return out;
    }

    private static Object dialogSet(HolderSet<Dialog> dialogs) {
        List<Object> out = new ArrayList<>();
        for (Holder<Dialog> holder : dialogs) {
            out.add(holder.unwrapKey().isPresent() ? (Object) holder.unwrapKey().get().identifier().toString() : dialog(holder.value()));
        }
        return out;
    }

    private static void common(Map<String, Object> out, CommonDialogData data) {
        out.put("title", component(data.title()));
        optional(out, "external_title", data.externalTitle(), each -> component((Component) each));
        out.put("can_close_with_escape", data.canCloseWithEscape());
        out.put("pause", data.pause());
        out.put("after_action", data.afterAction().getSerializedName());
        List<Object> body = new ArrayList<>();
        data.body().forEach(each -> body.add(body(each)));
        out.put("body", body);
        List<Object> inputs = new ArrayList<>();
        data.inputs().forEach(each -> inputs.add(input(each)));
        out.put("inputs", inputs);
    }

    private static Map<String, Object> button(ActionButton value) {
        Map<String, Object> out = new LinkedHashMap<>();
        CommonButtonData data = value.button();
        out.put("label", component(data.label()));
        optional(out, "tooltip", data.tooltip(), each -> component((Component) each));
        out.put("width", data.width());
        optional(out, "action", value.action(), each -> action((Action) each));
        return out;
    }

    @SuppressWarnings("unchecked")
    private static Object action(Action value) {
        if (value instanceof StaticAction fixed) {
            Map<String, Object> out = new LinkedHashMap<>();
            Object encoded = ClickEvent.CODEC.encodeStart(JavaOps.INSTANCE, fixed.value()).getOrThrow();
            Map<String, Object> source = (Map<String, Object>) encoded;
            out.put("type", "minecraft:" + source.get("action"));
            source.forEach((key, each) -> {
                if (!key.equals("action")) {
                    out.put(key, each);
                }
            });
            return out;
        }
        Map<String, Object> out = new LinkedHashMap<>();
        if (value instanceof CommandTemplate template) {
            out.put("type", "minecraft:dynamic/run_command");
            out.put("template", rawTemplate(template));
        } else if (value instanceof CustomAll custom) {
            out.put("type", "minecraft:dynamic/custom");
            out.put("id", custom.id().toString());
            optional(out, "additions", custom.additions(), each -> new Dynamic<>(net.minecraft.nbt.NbtOps.INSTANCE, (CompoundTag) each).convert(JavaOps.INSTANCE).getValue());
        } else {
            throw new IllegalArgumentException("Unknown dialog action " + value);
        }
        return out;
    }

    private static String rawTemplate(CommandTemplate template) {
        try {
            Object parsed = template.template();
            Field raw = parsed.getClass().getDeclaredField("raw");
            raw.setAccessible(true);
            return (String) raw.get(parsed);
        } catch (ReflectiveOperationException failure) {
            throw new IllegalStateException("Cannot read the command template", failure);
        }
    }

    private static Map<String, Object> body(DialogBody value) {
        Map<String, Object> out = new LinkedHashMap<>();
        if (value instanceof PlainMessage message) {
            out.put("type", "minecraft:plain_message");
            out.put("contents", component(message.contents()));
            out.put("width", message.width());
        } else if (value instanceof ItemBody item) {
            out.put("type", "minecraft:item");
            out.put("item", net.minecraft.world.item.ItemStackTemplate.CODEC.encodeStart(JavaOps.INSTANCE, item.item()).getOrThrow());
            optional(out, "description", item.description(), each -> {
                PlainMessage description = (PlainMessage) each;
                Map<String, Object> inner = new LinkedHashMap<>();
                inner.put("contents", component(description.contents()));
                inner.put("width", description.width());
                return inner;
            });
            out.put("show_decorations", item.showDecorations());
            out.put("show_tooltip", item.showTooltip());
            out.put("width", item.width());
            out.put("height", item.height());
        } else {
            throw new IllegalArgumentException("Unknown dialog body " + value);
        }
        return out;
    }

    private static Map<String, Object> input(Input value) {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("key", value.key());
        InputControl control = value.control();
        if (control instanceof BooleanInput flag) {
            out.put("type", "minecraft:boolean");
            out.put("label", component(flag.label()));
            out.put("initial", flag.initial());
            out.put("on_true", flag.onTrue());
            out.put("on_false", flag.onFalse());
        } else if (control instanceof NumberRangeInput range) {
            out.put("type", "minecraft:number_range");
            out.put("width", range.width());
            out.put("label", component(range.label()));
            out.put("label_format", range.labelFormat());
            out.put("start", range.rangeInfo().start());
            out.put("end", range.rangeInfo().end());
            optional(out, "initial", range.rangeInfo().initial(), each -> each);
            optional(out, "step", range.rangeInfo().step(), each -> each);
        } else if (control instanceof SingleOptionInput single) {
            out.put("type", "minecraft:single_option");
            out.put("width", single.width());
            List<Object> options = new ArrayList<>();
            for (SingleOptionInput.Entry entry : single.entries()) {
                Map<String, Object> each = new LinkedHashMap<>();
                each.put("id", entry.id());
                optional(each, "display", entry.display(), display -> component((Component) display));
                each.put("initial", entry.initial());
                options.add(each);
            }
            out.put("options", options);
            out.put("label", component(single.label()));
            out.put("label_visible", single.labelVisible());
        } else if (control instanceof TextInput text) {
            out.put("type", "minecraft:text");
            out.put("width", text.width());
            out.put("label", component(text.label()));
            out.put("label_visible", text.labelVisible());
            out.put("initial", text.initial());
            out.put("max_length", text.maxLength());
            optional(out, "multiline", text.multiline(), each -> {
                TextInput.MultilineOptions options = (TextInput.MultilineOptions) each;
                Map<String, Object> inner = new LinkedHashMap<>();
                optional(inner, "max_lines", options.maxLines(), v -> v);
                optional(inner, "height", options.height(), v -> v);
                return inner;
            });
        } else {
            throw new IllegalArgumentException("Unknown dialog input " + control);
        }
        return out;
    }
}
