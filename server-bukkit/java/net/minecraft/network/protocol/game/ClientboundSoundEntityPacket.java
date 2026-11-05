// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.entity.Entity;
public class ClientboundSoundEntityPacket implements Packet<ClientGamePacketListener> {
    private final Holder<SoundEvent> sound;
    private final SoundSource source;
    private final int id;
    private final float volume;
    private final float pitch;
    private final long seed;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSoundEntityPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSoundEntityPacket", () -> Packet.codec(ClientboundSoundEntityPacket::write, ClientboundSoundEntityPacket::new));
    public ClientboundSoundEntityPacket(Holder<SoundEvent> sound, SoundSource source, Entity sourceEntity, float volume, float pitch, long seed) {
        this.sound = sound;
        this.source = source;
        this.id = sourceEntity.getId();
        this.volume = volume;
        this.pitch = pitch;
        this.seed = seed;
    }
    private ClientboundSoundEntityPacket(RegistryFriendlyByteBuf input) {
        this.sound = SoundEvent.STREAM_CODEC.decode(input);
        this.source = input.readEnum(SoundSource.class);
        this.id = input.readVarInt();
        this.volume = input.readFloat();
        this.pitch = input.readFloat();
        this.seed = input.readLong();
    }
    private void write(RegistryFriendlyByteBuf output) {
        SoundEvent.STREAM_CODEC.encode(output, this.sound);
        output.writeEnum(this.source);
        output.writeVarInt(this.id);
        output.writeFloat(this.volume);
        output.writeFloat(this.pitch);
        output.writeLong(this.seed);
    }
    public PacketType<ClientboundSoundEntityPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SOUND_ENTITY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSoundEntityEvent(this);
    }
    public Holder<SoundEvent> getSound() {
        return sound;
    }
    public SoundSource getSource() {
        return source;
    }
    public int getId() {
        return id;
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
