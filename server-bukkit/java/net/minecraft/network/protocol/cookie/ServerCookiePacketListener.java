// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.cookie;

import net.minecraft.network.protocol.game.ServerPacketListener;
public interface ServerCookiePacketListener extends ServerPacketListener {
    void handleCookieResponse(ServerboundCookieResponsePacket packet);
}
