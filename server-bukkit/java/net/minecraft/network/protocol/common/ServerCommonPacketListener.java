// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;

import net.minecraft.network.protocol.cookie.ServerCookiePacketListener;
public interface ServerCommonPacketListener extends ServerCookiePacketListener {
    default void handleKeepAlive(ServerboundKeepAlivePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handleKeepAlive");
    }
    default void handlePong(ServerboundPongPacket serverboundPongPacket) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handlePong");
    }
    default void handleCustomPayload(ServerboundCustomPayloadPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handleCustomPayload");
    }
    default void handleResourcePackResponse(ServerboundResourcePackPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handleResourcePackResponse");
    }
    default void handleClientInformation(ServerboundClientInformationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handleClientInformation");
    }
    default void handleCustomClickAction(ServerboundCustomClickActionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.common.ServerCommonPacketListener#handleCustomClickAction");
    }
}
