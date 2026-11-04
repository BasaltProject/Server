// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundSetTitlesAnimationPacket implements Packet<ClientGamePacketListener> {
    private final int fadeIn;
    private final int stay;
    private final int fadeOut;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetTitlesAnimationPacket> STREAM_CODEC = Packet.codec(ClientboundSetTitlesAnimationPacket::write, ClientboundSetTitlesAnimationPacket::new);
    public ClientboundSetTitlesAnimationPacket(int fadeIn, int stay, int fadeOut) {
        this.fadeIn = fadeIn;
        this.stay = stay;
        this.fadeOut = fadeOut;
    }
    private ClientboundSetTitlesAnimationPacket(FriendlyByteBuf input) {
        this.fadeIn = input.readInt();
        this.stay = input.readInt();
        this.fadeOut = input.readInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeInt(this.fadeIn);
        output.writeInt(this.stay);
        output.writeInt(this.fadeOut);
    }
    public PacketType<ClientboundSetTitlesAnimationPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_TITLES_ANIMATION;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.setTitlesAnimation(this);
    }
    public int getFadeIn() {
        return fadeIn;
    }
    public int getStay() {
        return stay;
    }
    public int getFadeOut() {
        return fadeOut;
    }
}
