// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
import net.minecraft.sounds.SoundSource;
public class ClientboundStopSoundPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundStopSoundPacket> STREAM_CODEC = Packet.codec(ClientboundStopSoundPacket::write, ClientboundStopSoundPacket::new);
    private final Identifier name;
    private final SoundSource source;
    public ClientboundStopSoundPacket(Identifier name, SoundSource source) {
        this.name = name;
        this.source = source;
    }
    private ClientboundStopSoundPacket(FriendlyByteBuf input) {
        int flags = input.readByte();
        this.source = (flags & 1) > 0 ? input.readEnum(SoundSource.class) : null;
        this.name = (flags & 2) > 0 ? input.readIdentifier() : null;
    }
    private void write(FriendlyByteBuf output) {
        output.writeByte((source != null ? 1 : 0) | (name != null ? 2 : 0));
        if (source != null) {
            output.writeEnum(source);
        }
        if (name != null) {
            output.writeIdentifier(name);
        }
    }
    public PacketType<ClientboundStopSoundPacket> type() {
        return GamePacketTypes.CLIENTBOUND_STOP_SOUND;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleStopSoundEvent(this);
    }
    public Identifier getName() {
        return name;
    }
    public SoundSource getSource() {
        return source;
    }
}
