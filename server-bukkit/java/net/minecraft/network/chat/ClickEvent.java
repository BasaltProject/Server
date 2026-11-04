// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.io.File;
import java.net.URI;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.util.StringRepresentable;
import org.basaltmc.bukkit.facade.ChatJson;

public interface ClickEvent {
    Codec<ClickEvent> CODEC = Action.CODEC.<ClickEvent>dispatchMap("action", ClickEvent::action, Action::valueCodec).codec();

    Action action();

    record OpenUrl(URI uri) implements ClickEvent {
        public static final MapCodec<OpenUrl> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.STRING.xmap(URI::create, URI::toString).fieldOf("url").forGetter(OpenUrl::uri)).apply(instance, OpenUrl::new));

        @Override
        public Action action() {
            return Action.OPEN_URL;
        }
    }

    record OpenFile(String path) implements ClickEvent {
        public static final MapCodec<OpenFile> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.STRING.fieldOf("path").forGetter(OpenFile::path)).apply(instance, OpenFile::new));

        public OpenFile(File file) {
            this(file.getPath());
        }

        public OpenFile(Path path) {
            this(path.toFile());
        }

        public File file() {
            return new File(path);
        }

        @Override
        public Action action() {
            return Action.OPEN_FILE;
        }
    }

    record RunCommand(String command) implements ClickEvent {
        public static final MapCodec<RunCommand> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.STRING.fieldOf("command").forGetter(RunCommand::command)).apply(instance, RunCommand::new));

        @Override
        public Action action() {
            return Action.RUN_COMMAND;
        }
    }

    record SuggestCommand(String command) implements ClickEvent {
        public static final MapCodec<SuggestCommand> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.STRING.fieldOf("command").forGetter(SuggestCommand::command)).apply(instance, SuggestCommand::new));

        @Override
        public Action action() {
            return Action.SUGGEST_COMMAND;
        }
    }

    record ShowDialog(Holder<Dialog> dialog) implements ClickEvent {
        public static final MapCodec<ShowDialog> CODEC = ChatJson.unsupportedMap("show_dialog click events");

        @Override
        public Action action() {
            return Action.SHOW_DIALOG;
        }
    }

    record ChangePage(int page) implements ClickEvent {
        public static final MapCodec<ChangePage> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.INT.fieldOf("page").forGetter(ChangePage::page)).apply(instance, ChangePage::new));

        @Override
        public Action action() {
            return Action.CHANGE_PAGE;
        }
    }

    record CopyToClipboard(String value) implements ClickEvent {
        public static final MapCodec<CopyToClipboard> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.STRING.fieldOf("value").forGetter(CopyToClipboard::value)).apply(instance, CopyToClipboard::new));

        @Override
        public Action action() {
            return Action.COPY_TO_CLIPBOARD;
        }
    }

    record Custom(Identifier id, Optional<Tag> payload) implements ClickEvent {
        public static final MapCodec<Custom> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Identifier.CODEC.fieldOf("id").forGetter(Custom::id),
            ChatJson.TAG_CODEC.optionalFieldOf("payload").forGetter(Custom::payload)).apply(instance, Custom::new));

        @Override
        public Action action() {
            return Action.CUSTOM;
        }
    }

    enum Action implements StringRepresentable {
        OPEN_URL("open_url", true),
        OPEN_FILE("open_file", false),
        RUN_COMMAND("run_command", true),
        SUGGEST_COMMAND("suggest_command", true),
        SHOW_DIALOG("show_dialog", true),
        CHANGE_PAGE("change_page", true),
        COPY_TO_CLIPBOARD("copy_to_clipboard", true),
        CUSTOM("custom", true);

        public static final Codec<Action> UNSAFE_CODEC = StringRepresentable.fromEnum(Action::values);
        public static final Codec<Action> CODEC = UNSAFE_CODEC.validate(Action::filterForSerialization);
        private final boolean allowFromServer;
        private final String name;

        Action(String name, boolean allowFromServer) {
            this.name = name;
            this.allowFromServer = allowFromServer;
        }

        public boolean isAllowedFromServer() {
            return allowFromServer;
        }

        @Override
        public String getSerializedName() {
            return name;
        }

        public MapCodec<? extends ClickEvent> valueCodec() {
            return switch (this) {
                case OPEN_URL -> OpenUrl.CODEC;
                case OPEN_FILE -> OpenFile.CODEC;
                case RUN_COMMAND -> RunCommand.CODEC;
                case SUGGEST_COMMAND -> SuggestCommand.CODEC;
                case SHOW_DIALOG -> ShowDialog.CODEC;
                case CHANGE_PAGE -> ChangePage.CODEC;
                case COPY_TO_CLIPBOARD -> CopyToClipboard.CODEC;
                case CUSTOM -> Custom.CODEC;
            };
        }

        public static DataResult<Action> filterForSerialization(Action action) {
            return action.isAllowedFromServer() ? DataResult.success(action)
                : DataResult.error(() -> "Click event type not allowed: " + action);
        }
    }
}
