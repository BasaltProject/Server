// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
public class ClientboundUpdateMobEffectPacket implements Packet<ClientGamePacketListener> {
    private final int entityId;
    private final Holder<MobEffect> effect;
    private final int effectAmplifier;
    private final int effectDurationTicks;
    private final byte flags;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundUpdateMobEffectPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateMobEffectPacket", () -> Packet.codec(ClientboundUpdateMobEffectPacket::write, ClientboundUpdateMobEffectPacket::new));
    public ClientboundUpdateMobEffectPacket(int entityId, MobEffectInstance effect, boolean blend) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket#<init>");
    }
    private ClientboundUpdateMobEffectPacket(RegistryFriendlyByteBuf input) {
        this.entityId = input.readVarInt();
        this.effect = MobEffect.STREAM_CODEC.decode(input);
        this.effectAmplifier = input.readVarInt();
        this.effectDurationTicks = input.readVarInt();
        this.flags = input.readByte();
    }
    private void write(RegistryFriendlyByteBuf output) {
        output.writeVarInt(this.entityId);
        MobEffect.STREAM_CODEC.encode(output, this.effect);
        output.writeVarInt(this.effectAmplifier);
        output.writeVarInt(this.effectDurationTicks);
        output.writeByte(this.flags);
    }
    public PacketType<ClientboundUpdateMobEffectPacket> type() {
        return GamePacketTypes.CLIENTBOUND_UPDATE_MOB_EFFECT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleUpdateMobEffect(this);
    }
    public int getEntityId() {
        return entityId;
    }
    public Holder<MobEffect> getEffect() {
        return effect;
    }
    public int getEffectAmplifier() {
        return effectAmplifier;
    }
    public int getEffectDurationTicks() {
        return effectDurationTicks;
    }
    public boolean isEffectVisible() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket#isEffectVisible");
    }
    public boolean isEffectAmbient() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket#isEffectAmbient");
    }
    public boolean effectShowsIcon() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket#effectShowsIcon");
    }
    public boolean shouldBlend() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket#shouldBlend");
    }
}
