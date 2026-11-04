// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundSetExperiencePacket implements Packet<ClientGamePacketListener> {
    private final float experienceProgress;
    private final int totalExperience;
    private final int experienceLevel;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetExperiencePacket> STREAM_CODEC = Packet.codec(ClientboundSetExperiencePacket::write, ClientboundSetExperiencePacket::new);
    public ClientboundSetExperiencePacket(float experienceProgress, int totalExperience, int experienceLevel) {
        this.experienceProgress = experienceProgress;
        this.totalExperience = totalExperience;
        this.experienceLevel = experienceLevel;
    }
    private ClientboundSetExperiencePacket(FriendlyByteBuf input) {
        this.experienceProgress = input.readFloat();
        this.experienceLevel = input.readVarInt();
        this.totalExperience = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeFloat(this.experienceProgress);
        output.writeVarInt(this.experienceLevel);
        output.writeVarInt(this.totalExperience);
    }
    public PacketType<ClientboundSetExperiencePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_EXPERIENCE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetExperience(this);
    }
    public float getExperienceProgress() {
        return experienceProgress;
    }
    public int getTotalExperience() {
        return totalExperience;
    }
    public int getExperienceLevel() {
        return experienceLevel;
    }
}
