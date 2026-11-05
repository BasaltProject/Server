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
        this.sound = sound;
        this.source = source;
        this.x = (int) (x * LOCATION_ACCURACY);
        this.y = (int) (y * LOCATION_ACCURACY);
        this.z = (int) (z * LOCATION_ACCURACY);
        this.volume = volume;
        this.pitch = pitch;
        this.seed = seed;
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
        return x / LOCATION_ACCURACY;
    }
    public double getY() {
        return y / LOCATION_ACCURACY;
    }
    public double getZ() {
        return z / LOCATION_ACCURACY;
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
