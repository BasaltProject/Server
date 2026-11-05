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
    private static final int FLAG_AMBIENT = 1;
    private static final int FLAG_VISIBLE = 2;
    private static final int FLAG_SHOW_ICON = 4;
    private static final int FLAG_BLEND = 8;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundUpdateMobEffectPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateMobEffectPacket", () -> Packet.codec(ClientboundUpdateMobEffectPacket::write, ClientboundUpdateMobEffectPacket::new));
    public ClientboundUpdateMobEffectPacket(int entityId, MobEffectInstance effect, boolean blend) {
        this.entityId = entityId;
        this.effect = effect.getEffect();
        this.effectAmplifier = effect.getAmplifier();
        this.effectDurationTicks = effect.getDuration();
        byte bits = 0;
        if (effect.isAmbient()) {
            bits |= FLAG_AMBIENT;
        }
        if (effect.isVisible()) {
            bits |= FLAG_VISIBLE;
        }
        if (effect.showIcon()) {
            bits |= FLAG_SHOW_ICON;
        }
        if (blend) {
            bits |= FLAG_BLEND;
        }
        this.flags = bits;
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
        return (flags & FLAG_VISIBLE) != 0;
    }
    public boolean isEffectAmbient() {
        return (flags & FLAG_AMBIENT) != 0;
    }
    public boolean effectShowsIcon() {
        return (flags & FLAG_SHOW_ICON) != 0;
    }
    public boolean shouldBlend() {
        return (flags & FLAG_BLEND) != 0;
    }
}
