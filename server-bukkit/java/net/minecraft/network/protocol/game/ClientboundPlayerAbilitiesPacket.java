// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.player.Abilities;
public class ClientboundPlayerAbilitiesPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundPlayerAbilitiesPacket> STREAM_CODEC = Packet.codec(ClientboundPlayerAbilitiesPacket::write, ClientboundPlayerAbilitiesPacket::new);
    private final boolean invulnerable;
    private final boolean isFlying;
    private final boolean canFly;
    private final boolean instabuild;
    private final float flyingSpeed;
    private final float walkingSpeed;
    public ClientboundPlayerAbilitiesPacket(Abilities abilities) {
        this.invulnerable = abilities.invulnerable;
        this.isFlying = abilities.flying;
        this.canFly = abilities.mayfly;
        this.instabuild = abilities.instabuild;
        this.flyingSpeed = abilities.getFlyingSpeed();
        this.walkingSpeed = abilities.getWalkingSpeed();
    }
    private ClientboundPlayerAbilitiesPacket(FriendlyByteBuf input) {
        byte flags = input.readByte();
        this.invulnerable = (flags & 1) != 0;
        this.isFlying = (flags & 2) != 0;
        this.canFly = (flags & 4) != 0;
        this.instabuild = (flags & 8) != 0;
        this.flyingSpeed = input.readFloat();
        this.walkingSpeed = input.readFloat();
    }
    private void write(FriendlyByteBuf output) {
        output.writeByte((invulnerable ? 1 : 0) | (isFlying ? 2 : 0) | (canFly ? 4 : 0) | (instabuild ? 8 : 0));
        output.writeFloat(flyingSpeed);
        output.writeFloat(walkingSpeed);
    }
    public PacketType<ClientboundPlayerAbilitiesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_ABILITIES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handlePlayerAbilities(this);
    }
    public boolean isInvulnerable() {
        return invulnerable;
    }
    public boolean isFlying() {
        return isFlying;
    }
    public boolean canFly() {
        return canFly;
    }
    public boolean canInstabuild() {
        return instabuild;
    }
    public float getFlyingSpeed() {
        return flyingSpeed;
    }
    public float getWalkingSpeed() {
        return walkingSpeed;
    }
}
