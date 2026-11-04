// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.StringRepresentable;
public enum AdvancementType implements StringRepresentable {
    TASK(0, "task", ChatFormatting.GREEN),
    CHALLENGE(1, "challenge", ChatFormatting.DARK_PURPLE),
    GOAL(2, "goal", ChatFormatting.GREEN);
    public static final Codec<AdvancementType> CODEC = StringRepresentable.fromEnum(AdvancementType::values);
    public static final StreamCodec<ByteBuf, AdvancementType> STREAM_CODEC =
        ByteBufCodecs.idMapper(id -> id >= 0 && id < values().length ? values()[id] : TASK, type -> type.id);
    private final int id;
    private final String name;
    private final ChatFormatting chatColor;
    private final Component displayName;
    AdvancementType(int id, String name, ChatFormatting chatColor) {
        this.id = id;
        this.name = name;
        this.chatColor = chatColor;
        this.displayName = Component.translatable("advancements.toast." + name);
    }
    public ChatFormatting getChatColor() {
        return chatColor;
    }
    public Component getDisplayName() {
        return displayName;
    }
    @Override
    public String getSerializedName() {
        return name;
    }
    public MutableComponent createAnnouncement(AdvancementHolder holder, ServerPlayer player) {
        return Component.translatable("chat.type.advancement." + name, player.getDisplayName(), Advancement.name(holder));
    }
}
