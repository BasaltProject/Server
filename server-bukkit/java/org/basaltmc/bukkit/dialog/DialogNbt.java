package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.Dialog;
import io.papermc.paper.registry.data.dialog.ActionButton;
import io.papermc.paper.registry.data.dialog.DialogBase;
import io.papermc.paper.registry.data.dialog.action.DialogAction;
import io.papermc.paper.registry.data.dialog.body.DialogBody;
import io.papermc.paper.registry.data.dialog.body.ItemDialogBody;
import io.papermc.paper.registry.data.dialog.body.PlainMessageDialogBody;
import io.papermc.paper.registry.data.dialog.input.BooleanDialogInput;
import io.papermc.paper.registry.data.dialog.input.DialogInput;
import io.papermc.paper.registry.data.dialog.input.NumberRangeDialogInput;
import io.papermc.paper.registry.data.dialog.input.SingleOptionDialogInput;
import io.papermc.paper.registry.data.dialog.input.TextDialogInput;
import io.papermc.paper.registry.data.dialog.type.ConfirmationType;
import io.papermc.paper.registry.data.dialog.type.DialogListType;
import io.papermc.paper.registry.data.dialog.type.DialogType;
import io.papermc.paper.registry.data.dialog.type.MultiActionType;
import io.papermc.paper.registry.data.dialog.type.NoticeType;
import io.papermc.paper.registry.data.dialog.type.ServerLinksType;
import io.papermc.paper.registry.set.RegistryKeySet;
import io.papermc.paper.registry.set.RegistryValueSet;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.event.ClickEvent;
import org.basaltmc.bukkit.item.Texts;

/** A dialog written the way the game's own dialog data is, as the SNBT the server sends to the client. */
public final class DialogNbt {
    private DialogNbt() {}

    /** SNBT that is embedded as it is. */
    record Raw(String snbt) {}

    private static Map<String, Object> map(Object... pairs) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (int at = 0; at < pairs.length; at += 2) {
            if (pairs[at + 1] != null) {
                out.put((String) pairs[at], pairs[at + 1]);
            }
        }
        return out;
    }

    private static Object text(Component component) {
        return Texts.toNbt(component);
    }

    private static Map<String, Object> plain(PlainMessageDialogBody body) {
        return map("contents", text(body.contents()), "width", body.width());
    }

    private static Map<String, Object> body(DialogBody body) {
        if (body instanceof PlainMessageDialogBody message) {
            return map("type", "minecraft:plain_message", "contents", text(message.contents()), "width",
                message.width());
        }
        ItemDialogBody item = (ItemDialogBody) body;
        Map<String, Object> stack = map("id", item.item().getType().getKey().toString(), "count",
            item.item().getAmount());
        Map<String, Object> components = org.basaltmc.bukkit.Items.componentsOf(item.item());
        if (components != null && !components.isEmpty()) {
            stack.put("components", components);
        }
        return map("type", "minecraft:item", "item", stack, "description",
            item.description() == null ? null : plain(item.description()), "show_decorations", item.showDecorations(),
            "show_tooltip", item.showTooltip(), "width", item.width(), "height", item.height());
    }

    private static Map<String, Object> input(DialogInput input) {
        if (input instanceof BooleanDialogInput flag) {
            return map("type", "minecraft:boolean", "key", flag.key(), "label", text(flag.label()), "initial",
                flag.initial(), "on_true", flag.onTrue(), "on_false", flag.onFalse());
        }
        if (input instanceof NumberRangeDialogInput range) {
            return map("type", "minecraft:number_range", "key", range.key(), "width", range.width(), "label",
                text(range.label()), "label_format", range.labelFormat(), "start", range.start(), "end", range.end(),
                "initial", range.initial(), "step", range.step());
        }
        if (input instanceof SingleOptionDialogInput option) {
            List<Object> entries = new ArrayList<>();
            for (SingleOptionDialogInput.OptionEntry entry : option.entries()) {
                entries.add(map("id", entry.id(), "display", entry.display() == null ? null : text(entry.display()),
                    "initial", entry.initial()));
            }
            return map("type", "minecraft:single_option", "key", option.key(), "width", option.width(), "label",
                text(option.label()), "label_visible", option.labelVisible(), "options", entries);
        }
        TextDialogInput line = (TextDialogInput) input;
        TextDialogInput.MultilineOptions multiline = line.multiline();
        Map<String, Object> lines = multiline == null ? null
            : map("max_lines", multiline.maxLines(), "height", multiline.height());
        return map("type", "minecraft:text", "key", line.key(), "width", line.width(), "label", text(line.label()),
            "label_visible", line.labelVisible(), "initial", line.initial(), "max_length", line.maxLength(),
            "multiline", lines);
    }

    private static Object dialogRef(io.papermc.paper.dialog.Dialog dialog) {
        return dialog instanceof BasaltDialog inline ? inline.toData() : dialog.getKey().toString();
    }

    private static Map<String, Object> action(DialogAction action) {
        if (action instanceof DialogAction.CommandTemplateAction template) {
            return map("type", "minecraft:dynamic/run_command", "template", template.template());
        }
        if (action instanceof DialogAction.CustomClickAction custom) {
            return map("type", "minecraft:dynamic/custom", "id", custom.id().asString(), "additions",
                custom.additions() == null ? null : new Raw(custom.additions().string()));
        }
        ClickEvent<?> click = ((DialogAction.StaticAction) action).value();
        ClickEvent.Payload payload = click.payload();
        String name = click.action().toString();
        return switch (name) {
            case "open_url" -> map("type", "minecraft:open_url", "url", ((ClickEvent.Payload.Text) payload).value());
            case "run_command" -> map("type", "minecraft:run_command", "command",
                ((ClickEvent.Payload.Text) payload).value());
            case "suggest_command" -> map("type", "minecraft:suggest_command", "command",
                ((ClickEvent.Payload.Text) payload).value());
            case "change_page" -> map("type", "minecraft:change_page", "page",
                ((ClickEvent.Payload.Int) payload).integer());
            case "copy_to_clipboard" -> map("type", "minecraft:copy_to_clipboard", "value",
                ((ClickEvent.Payload.Text) payload).value());
            case "show_dialog" -> map("type", "minecraft:show_dialog", "dialog",
                dialogRef((io.papermc.paper.dialog.Dialog) ((ClickEvent.Payload.Dialog) payload).dialog()));
            case "custom" -> {
                ClickEvent.Payload.Custom custom = (ClickEvent.Payload.Custom) payload;
                yield map("type", "minecraft:custom", "id", custom.key().asString(), "payload",
                    custom.nbt() == null ? null : new Raw(custom.nbt().string()));
            }
            default -> throw new IllegalArgumentException("A dialog cannot run a " + name + " click");
        };
    }

    private static Map<String, Object> button(ActionButton button) {
        return map("label", text(button.label()), "tooltip", button.tooltip() == null ? null : text(button.tooltip()),
            "width", button.width(), "action", button.action() == null ? null : action(button.action()));
    }

    private static List<Object> dialogs(io.papermc.paper.registry.set.RegistrySet<Dialog> set) {
        List<Object> out = new ArrayList<>();
        if (set instanceof RegistryKeySet<Dialog> keys) {
            for (io.papermc.paper.registry.TypedKey<Dialog> key : keys.values()) {
                out.add(key.key().asString());
            }
        } else if (set instanceof RegistryValueSet<Dialog> values) {
            for (Dialog one : values.values()) {
                out.add(dialogRef(one));
            }
        }
        return out;
    }

    static Map<String, Object> dialog(DialogBase base, DialogType type) {
        List<Object> bodies = new ArrayList<>();
        for (DialogBody one : base.body()) {
            bodies.add(body(one));
        }
        List<Object> inputs = new ArrayList<>();
        for (DialogInput one : base.inputs()) {
            inputs.add(input(one));
        }
        Map<String, Object> out = new LinkedHashMap<>();
        if (type instanceof NoticeType notice) {
            out.put("type", "minecraft:notice");
            out.put("action", button(notice.action()));
        } else if (type instanceof ConfirmationType confirmation) {
            out.put("type", "minecraft:confirmation");
            out.put("yes", button(confirmation.yesButton()));
            out.put("no", button(confirmation.noButton()));
        } else if (type instanceof MultiActionType multi) {
            out.put("type", "minecraft:multi_action");
            List<Object> actions = new ArrayList<>();
            for (ActionButton one : multi.actions()) {
                actions.add(button(one));
            }
            out.put("actions", actions);
            if (multi.exitAction() != null) {
                out.put("exit_action", button(multi.exitAction()));
            }
            out.put("columns", multi.columns());
        } else if (type instanceof ServerLinksType links) {
            out.put("type", "minecraft:server_links");
            if (links.exitAction() != null) {
                out.put("exit_action", button(links.exitAction()));
            }
            out.put("columns", links.columns());
            out.put("button_width", links.buttonWidth());
        } else {
            DialogListType listing = (DialogListType) type;
            out.put("type", "minecraft:dialog_list");
            out.put("dialogs", dialogs(listing.dialogs()));
            if (listing.exitAction() != null) {
                out.put("exit_action", button(listing.exitAction()));
            }
            out.put("columns", listing.columns());
            out.put("button_width", listing.buttonWidth());
        }
        out.put("title", text(base.title()));
        if (base.externalTitle() != null) {
            out.put("external_title", text(base.externalTitle()));
        }
        out.put("can_close_with_escape", base.canCloseWithEscape());
        out.put("pause", base.pause());
        out.put("after_action", switch (base.afterAction()) {
            case CLOSE -> "close";
            case NONE -> "none";
            case WAIT_FOR_RESPONSE -> "wait_for_response";
        });
        out.put("body", bodies);
        out.put("inputs", inputs);
        return out;
    }

    /** The data as SNBT. */
    public static String snbt(Object value) {
        StringBuilder out = new StringBuilder();
        write(out, value);
        return out.toString();
    }

    private static void quote(StringBuilder out, String text) {
        out.append('"');
        for (int at = 0; at < text.length(); at++) {
            char one = text.charAt(at);
            if (one == '"' || one == '\\') {
                out.append('\\');
            }
            out.append(one);
        }
        out.append('"');
    }

    private static void write(StringBuilder out, Object value) {
        if (value instanceof Map<?, ?> map) {
            out.append('{');
            boolean first = true;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                if (!first) {
                    out.append(',');
                }
                first = false;
                quote(out, entry.getKey().toString());
                out.append(':');
                write(out, entry.getValue());
            }
            out.append('}');
        } else if (value instanceof List<?> list) {
            out.append('[');
            for (int at = 0; at < list.size(); at++) {
                if (at > 0) {
                    out.append(',');
                }
                write(out, list.get(at));
            }
            out.append(']');
        } else if (value instanceof String text) {
            quote(out, text);
        } else if (value instanceof Boolean flag) {
            out.append(flag ? "1b" : "0b");
        } else if (value instanceof Byte number) {
            out.append(number).append('b');
        } else if (value instanceof Short number) {
            out.append(number).append('s');
        } else if (value instanceof Integer number) {
            out.append(number);
        } else if (value instanceof Long number) {
            out.append(number).append('L');
        } else if (value instanceof Float number) {
            out.append(number).append('f');
        } else if (value instanceof Double number) {
            out.append(number).append('d');
        } else if (value instanceof Raw raw) {
            out.append(raw.snbt());
        } else if (value instanceof int[] numbers) {
            out.append("[I;");
            for (int at = 0; at < numbers.length; at++) {
                out.append(at > 0 ? "," : "").append(numbers[at]);
            }
            out.append(']');
        } else {
            throw new IllegalArgumentException("not an NBT value: " + value);
        }
    }
}
