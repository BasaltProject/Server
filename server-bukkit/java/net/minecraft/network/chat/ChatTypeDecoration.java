// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.IntFunction;
import net.minecraft.ChatFormatting;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ByIdMap;
import net.minecraft.util.StringRepresentable;
public record ChatTypeDecoration(String translationKey, List<Parameter> parameters, Style style) {
    public static final Codec<ChatTypeDecoration> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.STRING.fieldOf("translation_key").forGetter(ChatTypeDecoration::translationKey),
        Parameter.CODEC.listOf().fieldOf("parameters").forGetter(ChatTypeDecoration::parameters),
        Style.Serializer.CODEC.optionalFieldOf("style", Style.EMPTY).forGetter(ChatTypeDecoration::style)).apply(i, ChatTypeDecoration::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, ChatTypeDecoration> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.STRING_UTF8, ChatTypeDecoration::translationKey,
        Parameter.STREAM_CODEC.apply(ByteBufCodecs.list()), ChatTypeDecoration::parameters,
        Style.Serializer.TRUSTED_STREAM_CODEC, ChatTypeDecoration::style, ChatTypeDecoration::new);
    public static ChatTypeDecoration withSender(String translationKey) {
        return new ChatTypeDecoration(translationKey, List.of(Parameter.SENDER, Parameter.CONTENT), Style.EMPTY);
    }
    public static ChatTypeDecoration incomingDirectMessage(String translationKey) {
        return new ChatTypeDecoration(translationKey, List.of(Parameter.SENDER, Parameter.CONTENT), Style.EMPTY.withColor(ChatFormatting.GRAY).withItalic(true));
    }
    public static ChatTypeDecoration outgoingDirectMessage(String translationKey) {
        return new ChatTypeDecoration(translationKey, List.of(Parameter.TARGET, Parameter.CONTENT), Style.EMPTY.withColor(ChatFormatting.GRAY).withItalic(true));
    }
    public static ChatTypeDecoration teamMessage(String translationKey) {
        return new ChatTypeDecoration(translationKey, List.of(Parameter.TARGET, Parameter.SENDER, Parameter.CONTENT), Style.EMPTY);
    }
    public Component decorate(Component content, ChatType.Bound chatType) {
        Component[] resolved = new Component[parameters.size()];
        for (int i = 0; i < resolved.length; i++) {
            resolved[i] = parameters.get(i).select(content, chatType);
        }
        return Component.translatable(translationKey, (Object[]) resolved).withStyle(style);
    }
    public static enum Parameter implements StringRepresentable {
        SENDER(0, "sender", (content, chatType) -> chatType.name()),
        TARGET(1, "target", (content, chatType) -> chatType.targetName().orElse(CommonComponents.EMPTY)),
        CONTENT(2, "content", (content, chatType) -> content);
        private static final IntFunction<Parameter> BY_ID = ByIdMap.continuous(p -> p.id, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final Codec<Parameter> CODEC = StringRepresentable.fromEnum(Parameter::values);
        public static final StreamCodec<ByteBuf, Parameter> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, p -> p.id);
        private final int id;
        private final String name;
        private final Selector selector;
        private Parameter(int id, String name, Selector selector) {
            this.id = id;
            this.name = name;
            this.selector = selector;
        }
        public Component select(Component content, ChatType.Bound chatType) {
            return selector.select(content, chatType);
        }
        public String getSerializedName() {
            return name;
        }
        public interface Selector {
            Component select(Component content, ChatType.Bound chatType);
        }
    }
}
