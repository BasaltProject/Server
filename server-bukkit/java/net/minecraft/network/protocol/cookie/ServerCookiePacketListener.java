// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.cookie;

import net.minecraft.network.protocol.game.ServerPacketListener;
public interface ServerCookiePacketListener extends ServerPacketListener {
    default void handleCookieResponse(ServerboundCookieResponsePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.cookie.ServerCookiePacketListener#handleCookieResponse");
    }
}
