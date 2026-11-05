// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.ServerboundPacketListener;
import net.minecraft.ReportedException;
import net.minecraft.network.protocol.Packet;
import org.slf4j.Logger;
public interface ServerPacketListener extends ServerboundPacketListener {
    Logger LOGGER = org.slf4j.LoggerFactory.getLogger(ServerPacketListener.class);
    default void onPacketError(Packet packet, Exception e) throws ReportedException {
        LOGGER.error("Failed to handle packet {}, suppressing error", packet, e);
    }
}
