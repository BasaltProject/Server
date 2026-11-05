// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.brigadier.Message;
import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.network.chat.contents.KeybindContents;
import net.minecraft.network.chat.contents.PlainTextContents;
import net.minecraft.network.chat.contents.TranslatableContents;
import net.minecraft.network.chat.contents.data.DataSource;
import net.minecraft.network.chat.contents.objects.ObjectInfo;
import net.minecraft.resources.Identifier;
import net.minecraft.util.CompilableString;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.world.level.ChunkPos;

public interface Component extends FormattedText, Message {
    Style getStyle();

    ComponentContents getContents();

    List<Component> getSiblings();

    default net.kyori.adventure.text.Component asAdventure() {
        return org.basaltmc.bukkit.facade.ChatJson.toAdventure(this);
    }

    @Override
    default String getString() {
        StringBuilder builder = new StringBuilder();
        visit(content -> {
            builder.append(content);
            return Optional.empty();
        });
        return builder.toString();
    }

    default String getString(int limit) {
        StringBuilder builder = new StringBuilder();
        visit(content -> {
            int remaining = limit - builder.length();
            if (remaining <= 0) {
                return STOP_ITERATION;
            }
            builder.append(content.length() <= remaining ? content : content.substring(0, remaining));
            return Optional.empty();
        });
        return builder.toString();
    }

    default String tryCollapseToString() {
        if (getContents() instanceof PlainTextContents plain && getSiblings().isEmpty() && getStyle().isEmpty()) {
            return plain.text();
        }
        return null;
    }

    default MutableComponent plainCopy() {
        return MutableComponent.create(getContents());
    }

    default MutableComponent copy() {
        return new MutableComponent(getContents(), new ArrayList<>(getSiblings()), getStyle());
    }

    default FormattedCharSequence getVisualOrderText() {
        java.util.List<FormattedCharSequence> parts = new ArrayList<>();
        visit((style, text) -> {
            parts.add(FormattedCharSequence.forward(text, style));
            return Optional.empty();
        }, Style.EMPTY);
        return FormattedCharSequence.composite(parts);
    }

    @Override
    default <T> Optional<T> visit(FormattedText.StyledContentConsumer<T> output, Style parentStyle) {
        Style style = getStyle().applyTo(parentStyle);
        Optional<T> self = getContents().visit(output, style);
        if (self.isPresent()) {
            return self;
        }
        for (Component sibling : getSiblings()) {
            Optional<T> result = sibling.visit(output, style);
            if (result.isPresent()) {
                return result;
            }
        }
        return Optional.empty();
    }

    @Override
    default <T> Optional<T> visit(FormattedText.ContentConsumer<T> output) {
        Optional<T> self = getContents().visit(output);
        if (self.isPresent()) {
            return self;
        }
        for (Component sibling : getSiblings()) {
            Optional<T> result = sibling.visit(output);
            if (result.isPresent()) {
                return result;
            }
        }
        return Optional.empty();
    }

    default List<Component> toFlatList() {
        return toFlatList(Style.EMPTY);
    }

    default List<Component> toFlatList(Style rootStyle) {
        List<Component> result = new ArrayList<>();
        visit((style, content) -> {
            if (!content.isEmpty()) {
                result.add(literal(content).withStyle(style));
            }
            return Optional.empty();
        }, rootStyle);
        return result;
    }

    default boolean contains(Component other) {
        if (this == other) {
            return true;
        }
        List<Component> mine = toFlatList();
        List<Component> theirs = other.toFlatList(other.getStyle().applyTo(getStyle()));
        return Collections.indexOfSubList(mine, theirs) != -1;
    }

    static Component nullToEmpty(String text) {
        return text != null ? literal(text) : empty();
    }

    static MutableComponent literal(String text) {
        return MutableComponent.create(PlainTextContents.create(text));
    }

    static MutableComponent translatable(String key) {
        return MutableComponent.create(new TranslatableContents(key, null, TranslatableContents.NO_ARGS));
    }

    static MutableComponent translatable(String key, Object... args) {
        return MutableComponent.create(new TranslatableContents(key, null, args));
    }

    static MutableComponent translatableEscape(String key, Object... args) {
        Object[] escaped = new Object[args.length];
        for (int i = 0; i < args.length; i++) {
            Object arg = args[i];
            escaped[i] = TranslatableContents.isAllowedPrimitiveArgument(arg) || arg instanceof Component ? arg
                : String.valueOf(arg);
        }
        return translatable(key, escaped);
    }

    static MutableComponent translatableWithFallback(String key, String fallback) {
        return MutableComponent.create(new TranslatableContents(key, fallback, TranslatableContents.NO_ARGS));
    }

    static MutableComponent translatableWithFallback(String key, String fallback, Object... args) {
        return MutableComponent.create(new TranslatableContents(key, fallback, args));
    }

    static MutableComponent empty() {
        return MutableComponent.create(PlainTextContents.EMPTY);
    }

    static MutableComponent keybind(String name) {
        return MutableComponent.create(new KeybindContents(name));
    }

    static MutableComponent nbt(CompilableString<NbtPathArgument.NbtPath> nbtPath, boolean interpreting, boolean plain,
        Optional<Component> separator, DataSource dataSource) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.NbtContents(nbtPath, interpreting, plain, separator, dataSource));
    }

    static MutableComponent score(CompilableString<EntitySelector> pattern, String objective) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.ScoreContents(com.mojang.datafixers.util.Either.left(pattern), objective));
    }

    static MutableComponent score(String name, String objective) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.ScoreContents(com.mojang.datafixers.util.Either.right(name), objective));
    }

    static MutableComponent selector(CompilableString<EntitySelector> pattern, Optional<Component> separator) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.SelectorContents(pattern, separator));
    }

    static MutableComponent object(ObjectInfo info) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.ObjectContents(info, Optional.empty()));
    }

    static MutableComponent object(ObjectInfo info, Component fallback) {
        return MutableComponent.create(new net.minecraft.network.chat.contents.ObjectContents(info, Optional.of(fallback)));
    }

    static Component translationArg(Date date) {
        return literal(date.toString());
    }

    static Component translationArg(Message message) {
        return message instanceof Component component ? component : literal(message.getString());
    }

    static Component translationArg(UUID uuid) {
        return literal(uuid.toString());
    }

    static Component translationArg(Identifier id) {
        return literal(id.toString());
    }

    static Component translationArg(ChunkPos chunkPos) {
        return literal(chunkPos.toString());
    }

    static Component translationArg(URI uri) {
        return literal(uri.toString());
    }
}
