// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.UnaryOperator;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.component.TooltipProvider;

public class SignText {
    public static final int LINES = 4;
    private static final Codec<Component[]> kLineCodec = ComponentSerialization.CODEC.listOf(LINES, LINES)
        .xmap(list -> list.toArray(new Component[0]), array -> List.of(array));
    public static final Codec<SignText> CODEC = RecordCodecBuilder.create(instance -> instance.group(
        kLineCodec.fieldOf("messages").forGetter(text -> text.messages),
        kLineCodec.lenientOptionalFieldOf("filtered_messages").forGetter(text -> text.filteredMessages()),
        DyeColor.CODEC.fieldOf("color").orElse(DyeColor.BLACK).forGetter(text -> text.color),
        Codec.BOOL.fieldOf("has_glowing_text").orElse(false).forGetter(text -> text.hasGlowingText))
        .apply(instance, SignText::load));
    private static final StreamCodec<RegistryFriendlyByteBuf, Component[]> LINES_CODEC = StreamCodec.of(
        (buf, lines) -> {
            for (Component line : lines) {
                ComponentSerialization.STREAM_CODEC.encode(buf, line);
            }
        },
        buf -> {
            Component[] lines = new Component[LINES];
            for (int i = 0; i < LINES; i++) {
                lines[i] = ComponentSerialization.STREAM_CODEC.decode(buf);
            }
            return lines;
        });
    public static final StreamCodec<RegistryFriendlyByteBuf, SignText> STREAM_CODEC = net.minecraft.network.codec.StreamCodec.composite(
        LINES_CODEC, each -> each.messages,
        net.minecraft.network.codec.ByteBufCodecs.optional(LINES_CODEC), SignText::filteredMessages,
        DyeColor.STREAM_CODEC, each -> each.color,
        net.minecraft.network.codec.ByteBufCodecs.BOOL, each -> each.hasGlowingText,
        SignText::load);
    public static final TooltipProvider.Getter<SignText> FRONT_TEXT = createTooltip(Component.translatable("block.minecraft.sign.front"));
    public static final TooltipProvider.Getter<SignText> BACK_TEXT = createTooltip(Component.translatable("block.minecraft.sign.back"));
    public static final SignText EMPTY = new SignText();

    private final Component[] messages;
    private final Component[] filteredMessages;
    private final DyeColor color;
    private final boolean hasGlowingText;

    public SignText() {
        this(blank(), blank(), DyeColor.BLACK, false);
    }

    public SignText(List<Component> messages, List<Component> filteredMessages, DyeColor color, boolean hasGlowingText) {
        this(messages.toArray(new Component[0]), filteredMessages.toArray(new Component[0]), color, hasGlowingText);
    }

    private SignText(Component[] messages, Component[] filteredMessages, DyeColor color, boolean hasGlowingText) {
        this.messages = messages;
        this.filteredMessages = filteredMessages;
        this.color = color;
        this.hasGlowingText = hasGlowingText;
    }

    private static Component[] blank() {
        Component[] lines = new Component[LINES];
        Arrays.fill(lines, Component.empty());
        return lines;
    }

    private static SignText load(Component[] messages, Optional<Component[]> filtered, DyeColor color, boolean glowing) {
        return new SignText(messages, filtered.orElse(messages), color, glowing);
    }

    private Optional<Component[]> filteredMessages() {
        for (int i = 0; i < LINES; i++) {
            if (!filteredMessages[i].equals(messages[i])) {
                return Optional.of(filteredMessages);
            }
        }
        return Optional.empty();
    }

    public boolean hasGlowingText() {
        return hasGlowingText;
    }

    public SignText withGlowingText(boolean hasGlowingText) {
        return hasGlowingText == this.hasGlowingText ? this : new SignText(messages, filteredMessages, color, hasGlowingText);
    }

    public DyeColor getColor() {
        return color;
    }

    public SignText withColor(DyeColor color) {
        return color == this.color ? this : new SignText(messages, filteredMessages, color, hasGlowingText);
    }

    public Component getMessage(int index, boolean shouldFilter) {
        return (shouldFilter ? filteredMessages : messages)[index];
    }

    public SignText setMessage(int index, Component message) {
        return setMessage(index, message, message);
    }

    public SignText setMessage(int index, Component message, Component filteredMessage) {
        Component[] raw = messages.clone();
        Component[] filtered = filteredMessages.clone();
        raw[index] = message;
        filtered[index] = filteredMessage;
        return new SignText(raw, filtered, color, hasGlowingText);
    }

    public boolean hasMessage(boolean shouldFilter) {
        for (Component line : getMessages(shouldFilter)) {
            if (!line.getString().isEmpty()) {
                return true;
            }
        }
        return false;
    }

    public List<Component> getMessages(boolean shouldFilter) {
        return List.of(shouldFilter ? filteredMessages : messages);
    }

    public FormattedCharSequence[] getRenderMessages(boolean shouldFilter, Function<Component, FormattedCharSequence> prepare) {
        FormattedCharSequence[] out = new FormattedCharSequence[LINES];
        for (int i = 0; i < LINES; i++) {
            out[i] = prepare.apply(getMessage(i, shouldFilter));
        }
        return out;
    }

    public boolean hasAnyClickCommands(boolean shouldFilter) {
        for (Component line : getMessages(shouldFilter)) {
            if (line.getStyle().getClickEvent() != null) {
                return true;
            }
        }
        return false;
    }

    public boolean hasEditableText(boolean shouldFilter) {
        return hasMessage(shouldFilter);
    }

    @Override
    public boolean equals(Object object) {
        return object instanceof SignText other && hasGlowingText == other.hasGlowingText && color == other.color
            && Arrays.equals(messages, other.messages) && Arrays.equals(filteredMessages, other.filteredMessages);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(messages) * 31 + color.hashCode() + (hasGlowingText ? 1 : 0);
    }

    public Mutable asMutable() {
        return new Mutable(this);
    }

    public static TooltipProvider.Getter<SignText> createTooltip(Component title) {
        return text -> (context, consumer, flag, components) -> {
            consumer.accept(title);
        };
    }

    public static class Mutable {
        private SignText current;

        private Mutable(SignText original) {
            this.current = original;
        }

        public SignText asImmutable() {
            return current;
        }

        public Mutable setLine(int index, Component raw, Component filtered) {
            current = current.setMessage(index, raw, filtered);
            return this;
        }

        public Mutable setLine(int index, Component text) {
            current = current.setMessage(index, text);
            return this;
        }

        public Mutable modifyLine(int index, UnaryOperator<Component> modifier) {
            return setLine(index, modifier.apply(current.getMessage(index, false)), modifier.apply(current.getMessage(index, true)));
        }

        public Mutable modifyLines(UnaryOperator<Component> modifier) {
            for (int i = 0; i < LINES; i++) {
                modifyLine(i, modifier);
            }
            return this;
        }

        public Mutable setColor(DyeColor color) {
            current = current.withColor(color);
            return this;
        }

        public Mutable setTextGlowing(boolean flag) {
            current = current.withGlowingText(flag);
            return this;
        }
    }
}
