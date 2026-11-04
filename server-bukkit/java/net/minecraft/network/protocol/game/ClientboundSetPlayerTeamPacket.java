// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.world.scores.PlayerTeam;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.scores.Team;
import net.minecraft.world.scores.TeamColor;
public class ClientboundSetPlayerTeamPacket implements WirePacket, Packet<ClientGamePacketListener> {
    public enum Action { ADD, REMOVE }
    private final String name;
    private final int method;
    private final Optional<Parameters> parameters;
    private final Collection<String> players;
    public ClientboundSetPlayerTeamPacket(
        String name, int method, Optional<Parameters> parameters, Collection<String> players) {
        if (method < 0 || method > 4)
            throw new IllegalArgumentException("Invalid team mode");
        if ((method == 0 || method == 2) && parameters.isEmpty())
            throw new IllegalArgumentException("Missing team parameters");
        this.name = java.util.Objects.requireNonNull(name);
        this.method = method;
        this.parameters = parameters;
        this.players = List.copyOf(players);
    }
    private static ClientboundSetPlayerTeamPacket read(RegistryFriendlyByteBuf in) {
        String name = PacketCodec.readString(in);
        int method = in.readUnsignedByte();
        Optional<Parameters> parameters = method == 0 || method == 2 ? Optional.of(new Parameters(in)) : Optional.empty();
        List<String> players = new java.util.ArrayList<>();
        if (method == 0 || method == 3 || method == 4) {
            for (int count = PacketCodec.readVarInt(in); count > 0; count--) {
                players.add(PacketCodec.readString(in));
            }
        }
        return new ClientboundSetPlayerTeamPacket(name, method, parameters, players);
    }
    public static ClientboundSetPlayerTeamPacket createAddOrModifyPacket(PlayerTeam team, boolean add) {
        return new ClientboundSetPlayerTeamPacket(
            team.getName(), add ? 0 : 2, Optional.of(new Parameters(team)), add ? team.getPlayers() : List.of());
    }
    public static ClientboundSetPlayerTeamPacket createRemovePacket(PlayerTeam team) {
        return new ClientboundSetPlayerTeamPacket(team.getName(), 1, Optional.empty(), List.of());
    }
    public static ClientboundSetPlayerTeamPacket createPlayerPacket(PlayerTeam team, String player, Action action) {
        return new ClientboundSetPlayerTeamPacket(
            team.getName(), action == Action.ADD ? 3 : 4, Optional.empty(), List.of(player));
    }
    public int packetId() {
        return 0x6f;
    }
    public void write(ByteBuf out) {
        PacketCodec.string(out, name);
        out.writeByte(method);
        if (method == 0 || method == 2)
            parameters.orElseThrow().write(out);
        if (method == 0 || method == 3 || method == 4) {
            PacketCodec.varInt(out, players.size());
            for (String player : players) PacketCodec.string(out, player);
        }
    }
    public static final class Parameters {
        private final Component displayName, playerPrefix, playerSuffix;
        private final int options, visibility, collision, color;
        private Parameters(RegistryFriendlyByteBuf in) {
            displayName = net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC.decode(in);
            playerPrefix = net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC.decode(in);
            playerSuffix = net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC.decode(in);
            visibility = PacketCodec.readVarInt(in);
            collision = PacketCodec.readVarInt(in);
            color = in.readBoolean() ? PacketCodec.readVarInt(in) : -1;
            options = in.readByte();
        }
        public Component getDisplayName() {
            return displayName;
        }
        public Component getPlayerPrefix() {
            return playerPrefix;
        }
        public Component getPlayerSuffix() {
            return playerSuffix;
        }
        public int getOptions() {
            return options;
        }
        public Parameters(PlayerTeam team) {
            displayName = team.getDisplayName();
            playerPrefix = team.getPlayerPrefix();
            playerSuffix = team.getPlayerSuffix();
            options = team.packOptions();
            visibility = team.getNameTagVisibility().ordinal();
            collision = team.getCollisionRule().ordinal();
            color = team.getColor().map(net.minecraft.world.scores.TeamColor::id).orElse(-1);
        }
        public void write(ByteBuf out) {
            PacketCodec.component(out, displayName);
            PacketCodec.component(out, playerPrefix);
            PacketCodec.component(out, playerSuffix);
            PacketCodec.varInt(out, visibility);
            PacketCodec.varInt(out, collision);
            out.writeBoolean(color >= 0);
            if (color >= 0)
                PacketCodec.varInt(out, color);
            out.writeByte(options);
        }
            public static final StreamCodec<RegistryFriendlyByteBuf, Parameters> STREAM_CODEC = StreamCodec.of((buf, parameters) -> parameters.write(buf), Parameters::new);
}
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetPlayerTeamPacket> STREAM_CODEC = StreamCodec.of((buf, packet) -> packet.write(buf), ClientboundSetPlayerTeamPacket::read);
    public Action getPlayerAction() {
        return switch (method) {
            case 0, 3 -> Action.ADD;
            case 4 -> Action.REMOVE;
            default -> null;
        };
    }
    public Action getTeamAction() {
        return switch (method) {
            case 0 -> Action.ADD;
            case 1 -> Action.REMOVE;
            default -> null;
        };
    }
    public PacketType<ClientboundSetPlayerTeamPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_PLAYER_TEAM;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetPlayerTeamPacket(this);
    }
    public String getName() {
        return name;
    }
    public Collection<String> getPlayers() {
        return players;
    }
    public Optional<Parameters> getParameters() {
        return parameters;
    }
}
