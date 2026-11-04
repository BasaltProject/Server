// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
public class ClientboundSoundPacket implements Packet<ClientGamePacketListener> {
    private final Holder<SoundEvent> sound;
    private final SoundSource source;
    private final int x;
    private final int y;
    private final int z;
    private final float volume;
    private final float pitch;
    private final long seed;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSoundPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSoundPacket", () -> Packet.codec(ClientboundSoundPacket::write, ClientboundSoundPacket::new));
    public static final float LOCATION_ACCURACY = 8.0F;
    public ClientboundSoundPacket(Holder<SoundEvent> sound, SoundSource source, double x, double y, double z, float volume, float pitch, long seed) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSoundPacket#<init>");
    }
    private ClientboundSoundPacket(RegistryFriendlyByteBuf input) {
        this.sound = SoundEvent.STREAM_CODEC.decode(input);
        this.source = input.readEnum(SoundSource.class);
        this.x = input.readInt();
        this.y = input.readInt();
        this.z = input.readInt();
        this.volume = input.readFloat();
        this.pitch = input.readFloat();
        this.seed = input.readLong();
    }
    private void write(RegistryFriendlyByteBuf output) {
        SoundEvent.STREAM_CODEC.encode(output, this.sound);
        output.writeEnum(this.source);
        output.writeInt(this.x);
        output.writeInt(this.y);
        output.writeInt(this.z);
        output.writeFloat(this.volume);
        output.writeFloat(this.pitch);
        output.writeLong(this.seed);
    }
    public PacketType<ClientboundSoundPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SOUND;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSoundEvent(this);
    }
    public Holder<SoundEvent> getSound() {
        return sound;
    }
    public SoundSource getSource() {
        return source;
    }
    public double getX() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSoundPacket#getX");
    }
    public double getY() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSoundPacket#getY");
    }
    public double getZ() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSoundPacket#getZ");
    }
    public float getVolume() {
        return volume;
    }
    public float getPitch() {
        return pitch;
    }
    public long getSeed() {
        return seed;
    }
}
