// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.ItemStackTemplate;
import org.basaltmc.bukkit.facade.ChatJson;

public interface HoverEvent {
    Codec<HoverEvent> CODEC = Action.CODEC.<HoverEvent>dispatchMap("action", HoverEvent::action, Action::valueCodec).codec();

    Action action();

    record ShowText(Component value) implements HoverEvent {
        public static final MapCodec<ShowText> CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Codec.lazyInitialized(() -> ComponentSerialization.CODEC).fieldOf("value").forGetter(ShowText::value))
            .apply(instance, ShowText::new));

        @Override
        public Action action() {
            return Action.SHOW_TEXT;
        }
    }

    record ShowItem(ItemStackTemplate item) implements HoverEvent {
        public static final MapCodec<ShowItem> CODEC = ChatJson.unsupportedMap("show_item hover events");

        @Override
        public Action action() {
            return Action.SHOW_ITEM;
        }
    }

    record ShowEntity(EntityTooltipInfo entity) implements HoverEvent {
        public static final MapCodec<ShowEntity> CODEC = ChatJson.unsupportedMap("show_entity hover events");

        @Override
        public Action action() {
            return Action.SHOW_ENTITY;
        }
    }

    class EntityTooltipInfo {
        public static final MapCodec<EntityTooltipInfo> CODEC = ChatJson.unsupportedMap("entity tooltips");
        public final EntityType<?> type;
        public final UUID uuid;
        public final Optional<Component> name;

        public EntityTooltipInfo(EntityType<?> type, UUID uuid, Component name) {
            this(type, uuid, Optional.of(name));
        }

        public EntityTooltipInfo(EntityType<?> type, UUID uuid, Optional<Component> name) {
            this.type = type;
            this.uuid = uuid;
            this.name = name;
        }

        public List<Component> getTooltipLines() {
            List<Component> lines = new java.util.ArrayList<>();
            name.ifPresent(lines::add);
            lines.add(Component.literal(String.valueOf(type)));
            lines.add(Component.literal(uuid.toString()));
            return lines;
        }

        @Override
        public boolean equals(Object o) {
            return this == o || o instanceof EntityTooltipInfo other && type == other.type && uuid.equals(other.uuid)
                && name.equals(other.name);
        }

        @Override
        public int hashCode() {
            return Objects.hash(type, uuid, name);
        }
    }

    enum Action implements StringRepresentable {
        SHOW_TEXT("show_text", true),
        SHOW_ITEM("show_item", true),
        SHOW_ENTITY("show_entity", true);

        public static final Codec<Action> UNSAFE_CODEC = StringRepresentable.fromEnum(Action::values);
        public static final Codec<Action> CODEC = UNSAFE_CODEC;
        private final String name;
        private final boolean allowFromServer;

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

        @Override
        public String toString() {
            return "<action " + name + ">";
        }

        public MapCodec<? extends HoverEvent> valueCodec() {
            return switch (this) {
                case SHOW_TEXT -> ShowText.CODEC;
                case SHOW_ITEM -> ShowItem.CODEC;
                case SHOW_ENTITY -> ShowEntity.CODEC;
            };
        }
    }
}
