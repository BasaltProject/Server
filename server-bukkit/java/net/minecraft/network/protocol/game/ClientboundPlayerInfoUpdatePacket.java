// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import com.mojang.authlib.GameProfile;
import com.mojang.authlib.properties.PropertyMap;
import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.List;
import java.util.UUID;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.RemoteChatSession;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.GameType;

public class ClientboundPlayerInfoUpdatePacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundPlayerInfoUpdatePacket> STREAM_CODEC =
        Packet.codec(ClientboundPlayerInfoUpdatePacket::write, ClientboundPlayerInfoUpdatePacket::new);
    private final EnumSet<Action> actions;
    private final List<Entry> entries;

    public ClientboundPlayerInfoUpdatePacket(EnumSet<Action> actions, Collection<ServerPlayer> players) {
        this.actions = actions;
        this.entries = players.stream().map(Entry::new).toList();
    }

    public ClientboundPlayerInfoUpdatePacket(Action action, ServerPlayer player) {
        this.actions = EnumSet.of(action);
        this.entries = List.of(new Entry(player));
    }

    public static ClientboundPlayerInfoUpdatePacket createPlayerInitializing(Collection<ServerPlayer> players) {
        EnumSet<Action> actions = EnumSet.of(Action.ADD_PLAYER, Action.UPDATE_LISTED, Action.UPDATE_LATENCY,
            Action.UPDATE_GAME_MODE, Action.UPDATE_DISPLAY_NAME, Action.UPDATE_HAT, Action.UPDATE_LIST_ORDER);
        return new ClientboundPlayerInfoUpdatePacket(actions, players);
    }

    private ClientboundPlayerInfoUpdatePacket(RegistryFriendlyByteBuf input) {
        this.actions = input.readEnumSet(Action.class);
        int count = input.readVarInt();
        this.entries = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            EntryBuilder builder = new EntryBuilder(input.readUUID());
            for (Action action : actions) {
                action.reader.read(builder, input);
            }
            entries.add(builder.build());
        }
    }

    private void write(RegistryFriendlyByteBuf output) {
        output.writeEnumSet(actions, Action.class);
        output.writeVarInt(entries.size());
        for (Entry entry : entries) {
            output.writeUUID(entry.profileId());
            for (Action action : actions) {
                action.writer.write(output, entry);
            }
        }
    }

    public PacketType<ClientboundPlayerInfoUpdatePacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_INFO_UPDATE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handlePlayerInfoUpdate(this);
    }

    public EnumSet<Action> actions() {
        return actions;
    }

    public List<Entry> entries() {
        return entries;
    }

    public List<Entry> newEntries() {
        return actions.contains(Action.ADD_PLAYER) ? entries : List.of();
    }

    public String toString() {
        return "ClientboundPlayerInfoUpdatePacket{actions=" + actions + ", entries=" + entries + "}";
    }

    public static enum Action {
        ADD_PLAYER((entry, buf) -> entry.profile = new GameProfile(entry.profileId, buf.readUtf(16),
            ByteBufCodecs.GAME_PROFILE_PROPERTIES.decode(buf)), (buf, entry) -> {
            buf.writeUtf(entry.profile().name(), 16);
            ByteBufCodecs.GAME_PROFILE_PROPERTIES.encode(buf, entry.profile().properties());
        }),
        INITIALIZE_CHAT((entry, buf) -> entry.chatSession = buf.readBoolean() ? RemoteChatSession.Data.STREAM_CODEC.decode(buf) : null,
            (buf, entry) -> {
                buf.writeBoolean(entry.chatSession() != null);
                if (entry.chatSession() != null) {
                    RemoteChatSession.Data.STREAM_CODEC.encode(buf, entry.chatSession());
                }
            }),
        UPDATE_GAME_MODE((entry, buf) -> entry.gameMode = GameType.byId(buf.readVarInt()),
            (buf, entry) -> buf.writeVarInt(entry.gameMode().getId())),
        UPDATE_LISTED((entry, buf) -> entry.listed = buf.readBoolean(), (buf, entry) -> buf.writeBoolean(entry.listed())),
        UPDATE_LATENCY((entry, buf) -> entry.latency = buf.readVarInt(), (buf, entry) -> buf.writeVarInt(entry.latency())),
        UPDATE_DISPLAY_NAME((entry, buf) -> entry.displayName = buf.readBoolean() ? ComponentSerialization.TRUSTED_STREAM_CODEC.decode(buf) : null,
            (buf, entry) -> {
                buf.writeBoolean(entry.displayName() != null);
                if (entry.displayName() != null) {
                    ComponentSerialization.TRUSTED_STREAM_CODEC.encode(buf, entry.displayName());
                }
            }),
        UPDATE_LIST_ORDER((entry, buf) -> entry.listOrder = buf.readVarInt(), (buf, entry) -> buf.writeVarInt(entry.listOrder())),
        UPDATE_HAT((entry, buf) -> entry.showHat = buf.readBoolean(), (buf, entry) -> buf.writeBoolean(entry.showHat()));

        public static final StreamCodec<ByteBuf, Action> STREAM_CODEC = ByteBufCodecs.idMapper(id -> values()[Math.floorMod(id, values().length)],
            Action::ordinal);
        final Reader reader;
        final Writer writer;

        Action(Reader reader, Writer writer) {
            this.reader = reader;
            this.writer = writer;
        }

        public interface Reader {
            void read(EntryBuilder entry, RegistryFriendlyByteBuf input);
        }

        public interface Writer {
            void write(RegistryFriendlyByteBuf output, Entry entry);
        }
    }

    public static record Entry(UUID profileId, GameProfile profile, boolean listed, int latency, GameType gameMode, Component displayName,
        boolean showHat, int listOrder, RemoteChatSession.Data chatSession) {
        private Entry(ServerPlayer player) {
            this(player.getUUID(), new GameProfile(player.getUUID(), player.getName().getString(), new PropertyMap(
                com.google.common.collect.ImmutableMultimap.of())), true, bukkitPing(player), bukkitMode(player), null, true, 0, null);
        }

        private static int bukkitPing(ServerPlayer player) {
            return player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkit ? bukkit.getPing() : 0;
        }

        private static GameType bukkitMode(ServerPlayer player) {
            return player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkit
                ? GameType.byId(bukkit.getGameMode().getValue()) : GameType.SURVIVAL;
        }
    }

    static class EntryBuilder {
        final UUID profileId;
        GameProfile profile;
        boolean listed;
        int latency;
        GameType gameMode = GameType.DEFAULT_MODE;
        Component displayName;
        boolean showHat;
        int listOrder;
        RemoteChatSession.Data chatSession;

        private EntryBuilder(UUID profileId) {
            this.profileId = profileId;
            this.profile = new GameProfile(profileId, "");
        }

        Entry build() {
            return new Entry(profileId, profile, listed, latency, gameMode, displayName, showHat, listOrder, chatSession);
        }
    }
}
