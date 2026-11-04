// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.inventory.MenuType;
public class ClientboundOpenScreenPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundOpenScreenPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundOpenScreenPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.CONTAINER_ID, ClientboundOpenScreenPacket::getContainerId, ByteBufCodecs.registry(Registries.MENU), ClientboundOpenScreenPacket::getType, ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundOpenScreenPacket::getTitle, ClientboundOpenScreenPacket::new));
    public ClientboundOpenScreenPacket(int containerId, MenuType<?> type, Component title) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundOpenScreenPacket#<init>");
    }
    public PacketType<ClientboundOpenScreenPacket> type() {
        return GamePacketTypes.CLIENTBOUND_OPEN_SCREEN;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleOpenScreen(this);
    }
    public int getContainerId() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundOpenScreenPacket#getContainerId");
    }
    public MenuType<?> getType() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundOpenScreenPacket#getType");
    }
    public Component getTitle() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundOpenScreenPacket#getTitle");
    }
}
