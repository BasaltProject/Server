// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.PacketType;
public class CommonPacketTypes {
    public static final PacketType<ClientboundClearDialogPacket> CLIENTBOUND_CLEAR_DIALOG = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("clear_dialog"));
    public static final PacketType<ClientboundCustomPayloadPacket> CLIENTBOUND_CUSTOM_PAYLOAD = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_payload"));
    public static final PacketType<ClientboundCustomReportDetailsPacket> CLIENTBOUND_CUSTOM_REPORT_DETAILS = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_report_details"));
    public static final PacketType<ClientboundDisconnectPacket> CLIENTBOUND_DISCONNECT = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("disconnect"));
    public static final PacketType<ClientboundKeepAlivePacket> CLIENTBOUND_KEEP_ALIVE = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("keep_alive"));
    public static final PacketType<ClientboundPingPacket> CLIENTBOUND_PING = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("ping"));
    public static final PacketType<ClientboundResourcePackPopPacket> CLIENTBOUND_RESOURCE_PACK_POP = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("resource_pack_pop"));
    public static final PacketType<ClientboundResourcePackPushPacket> CLIENTBOUND_RESOURCE_PACK_PUSH = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("resource_pack_push"));
    public static final PacketType<ClientboundPostEffectsPacket> CLIENTBOUND_POST_EFFECTS = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("post_effects"));
    public static final PacketType<ClientboundServerLinksPacket> CLIENTBOUND_SERVER_LINKS = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("server_links"));
    public static final PacketType<ClientboundShowDialogPacket> CLIENTBOUND_SHOW_DIALOG = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("show_dialog"));
    public static final PacketType<ClientboundStoreCookiePacket> CLIENTBOUND_STORE_COOKIE = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("store_cookie"));
    public static final PacketType<ClientboundTransferPacket> CLIENTBOUND_TRANSFER = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("transfer"));
    public static final PacketType<ClientboundUpdateTagsPacket> CLIENTBOUND_UPDATE_TAGS = new PacketType<>(PacketFlow.CLIENTBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("update_tags"));
    public static final PacketType<ServerboundClientInformationPacket> SERVERBOUND_CLIENT_INFORMATION = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("client_information"));
    public static final PacketType<ServerboundCustomPayloadPacket> SERVERBOUND_CUSTOM_PAYLOAD = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_payload"));
    public static final PacketType<ServerboundKeepAlivePacket> SERVERBOUND_KEEP_ALIVE = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("keep_alive"));
    public static final PacketType<ServerboundPongPacket> SERVERBOUND_PONG = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("pong"));
    public static final PacketType<ServerboundResourcePackPacket> SERVERBOUND_RESOURCE_PACK = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("resource_pack"));
    public static final PacketType<ServerboundCustomClickActionPacket> SERVERBOUND_CUSTOM_CLICK_ACTION = new PacketType<>(PacketFlow.SERVERBOUND, net.minecraft.resources.Identifier.withDefaultNamespace("custom_click_action"));
}
