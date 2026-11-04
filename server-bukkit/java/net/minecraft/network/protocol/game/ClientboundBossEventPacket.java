// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import java.util.UUID;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.BossEvent;

public class ClientboundBossEventPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundBossEventPacket> STREAM_CODEC =
        Packet.codec(ClientboundBossEventPacket::write, ClientboundBossEventPacket::new);
    private static final int kDarkenScreen = 1;
    private static final int kPlayMusic = 2;
    private static final int kCreateWorldFog = 4;
    private final UUID id;
    private final Operation operation;

    private ClientboundBossEventPacket(UUID id, Operation operation) {
        this.id = id;
        this.operation = operation;
    }

    private ClientboundBossEventPacket(RegistryFriendlyByteBuf input) {
        this.id = input.readUUID();
        OperationType type = OperationType.values()[input.readVarInt() % OperationType.values().length];
        this.operation = switch (type) {
            case ADD -> new AddOperation(input);
            case REMOVE -> REMOVE_OPERATION;
            case UPDATE_PROGRESS -> new UpdateProgressOperation(input.readFloat());
            case UPDATE_NAME -> new UpdateNameOperation(ComponentSerialization.TRUSTED_STREAM_CODEC.decode(input));
            case UPDATE_STYLE -> new UpdateStyleOperation(input.readEnum(BossEvent.BossBarColor.class), input.readEnum(BossEvent.BossBarOverlay.class));
            case UPDATE_PROPERTIES -> UpdatePropertiesOperation.read(input);
        };
    }

    public static ClientboundBossEventPacket createAddPacket(BossEvent event) {
        return new ClientboundBossEventPacket(event.getId(), new AddOperation(event));
    }

    public static ClientboundBossEventPacket createRemovePacket(UUID id) {
        return new ClientboundBossEventPacket(id, REMOVE_OPERATION);
    }

    public static ClientboundBossEventPacket createUpdateProgressPacket(BossEvent event) {
        return new ClientboundBossEventPacket(event.getId(), new UpdateProgressOperation(event.getProgress()));
    }

    public static ClientboundBossEventPacket createUpdateNamePacket(BossEvent event) {
        return new ClientboundBossEventPacket(event.getId(), new UpdateNameOperation(event.getName()));
    }

    public static ClientboundBossEventPacket createUpdateStylePacket(BossEvent event) {
        return new ClientboundBossEventPacket(event.getId(), new UpdateStyleOperation(event.getColor(), event.getOverlay()));
    }

    public static ClientboundBossEventPacket createUpdatePropertiesPacket(BossEvent event) {
        return new ClientboundBossEventPacket(event.getId(),
            new UpdatePropertiesOperation(event.shouldDarkenScreen(), event.shouldPlayBossMusic(), event.shouldCreateWorldFog()));
    }

    private void write(RegistryFriendlyByteBuf output) {
        output.writeUUID(id);
        output.writeEnum(operation.getType());
        operation.write(output);
    }

    public PacketType<ClientboundBossEventPacket> type() {
        return GamePacketTypes.CLIENTBOUND_BOSS_EVENT;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleBossUpdate(this);
    }

    public void dispatch(Handler handler) {
        operation.dispatch(id, handler);
    }

    static enum OperationType {
        ADD, REMOVE, UPDATE_PROGRESS, UPDATE_NAME, UPDATE_STYLE, UPDATE_PROPERTIES;
    }

    public interface Handler {
        default void add(UUID id, Component name, float progress, BossEvent.BossBarColor color, BossEvent.BossBarOverlay overlay,
            boolean darkenScreen, boolean playMusic, boolean createWorldFog) {}

        default void remove(UUID id) {}

        default void updateProgress(UUID id, float progress) {}

        default void updateName(UUID id, Component name) {}

        default void updateStyle(UUID id, BossEvent.BossBarColor color, BossEvent.BossBarOverlay overlay) {}

        default void updateProperties(UUID id, boolean darkenScreen, boolean playMusic, boolean createWorldFog) {}
    }

    private static int packFlags(boolean darkenScreen, boolean playMusic, boolean createWorldFog) {
        return (darkenScreen ? kDarkenScreen : 0) | (playMusic ? kPlayMusic : 0) | (createWorldFog ? kCreateWorldFog : 0);
    }

    static final Operation REMOVE_OPERATION = new Operation() {
        public OperationType getType() {
            return OperationType.REMOVE;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.remove(id);
        }

        public void write(RegistryFriendlyByteBuf output) {}
    };

    static class AddOperation implements Operation {
        private final Component name;
        private final float progress;
        private final BossEvent.BossBarColor color;
        private final BossEvent.BossBarOverlay overlay;
        private final boolean darkenScreen;
        private final boolean playMusic;
        private final boolean createWorldFog;

        private AddOperation(BossEvent event) {
            this.name = event.getName();
            this.progress = event.getProgress();
            this.color = event.getColor();
            this.overlay = event.getOverlay();
            this.darkenScreen = event.shouldDarkenScreen();
            this.playMusic = event.shouldPlayBossMusic();
            this.createWorldFog = event.shouldCreateWorldFog();
        }

        private AddOperation(RegistryFriendlyByteBuf input) {
            this.name = ComponentSerialization.TRUSTED_STREAM_CODEC.decode(input);
            this.progress = input.readFloat();
            this.color = input.readEnum(BossEvent.BossBarColor.class);
            this.overlay = input.readEnum(BossEvent.BossBarOverlay.class);
            int flags = input.readUnsignedByte();
            this.darkenScreen = (flags & kDarkenScreen) != 0;
            this.playMusic = (flags & kPlayMusic) != 0;
            this.createWorldFog = (flags & kCreateWorldFog) != 0;
        }

        public OperationType getType() {
            return OperationType.ADD;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.add(id, name, progress, color, overlay, darkenScreen, playMusic, createWorldFog);
        }

        public void write(RegistryFriendlyByteBuf output) {
            ComponentSerialization.TRUSTED_STREAM_CODEC.encode(output, name);
            output.writeFloat(progress);
            output.writeEnum(color);
            output.writeEnum(overlay);
            output.writeByte(packFlags(darkenScreen, playMusic, createWorldFog));
        }
    }

    static record UpdateProgressOperation(float progress) implements Operation {
        public OperationType getType() {
            return OperationType.UPDATE_PROGRESS;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.updateProgress(id, progress);
        }

        public void write(RegistryFriendlyByteBuf output) {
            output.writeFloat(progress);
        }
    }

    static record UpdateNameOperation(Component name) implements Operation {
        public OperationType getType() {
            return OperationType.UPDATE_NAME;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.updateName(id, name);
        }

        public void write(RegistryFriendlyByteBuf output) {
            ComponentSerialization.TRUSTED_STREAM_CODEC.encode(output, name);
        }
    }

    static record UpdateStyleOperation(BossEvent.BossBarColor color, BossEvent.BossBarOverlay overlay) implements Operation {
        public OperationType getType() {
            return OperationType.UPDATE_STYLE;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.updateStyle(id, color, overlay);
        }

        public void write(RegistryFriendlyByteBuf output) {
            output.writeEnum(color);
            output.writeEnum(overlay);
        }
    }

    static record UpdatePropertiesOperation(boolean darkenScreen, boolean playMusic, boolean createWorldFog) implements Operation {
        static UpdatePropertiesOperation read(RegistryFriendlyByteBuf input) {
            int flags = input.readUnsignedByte();
            return new UpdatePropertiesOperation((flags & kDarkenScreen) != 0, (flags & kPlayMusic) != 0, (flags & kCreateWorldFog) != 0);
        }

        public OperationType getType() {
            return OperationType.UPDATE_PROPERTIES;
        }

        public void dispatch(UUID id, Handler handler) {
            handler.updateProperties(id, darkenScreen, playMusic, createWorldFog);
        }

        public void write(RegistryFriendlyByteBuf output) {
            output.writeByte(packFlags(darkenScreen, playMusic, createWorldFog));
        }
    }

    interface Operation {
        OperationType getType();

        void dispatch(UUID id, Handler handler);

        void write(RegistryFriendlyByteBuf output);
    }
}
