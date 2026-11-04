// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.ServerboundPacketListener;
import net.minecraft.ReportedException;
import net.minecraft.network.protocol.Packet;
import org.slf4j.Logger;
public interface ServerPacketListener extends ServerboundPacketListener {
    Logger LOGGER = null;
    default void onPacketError(Packet packet, Exception e) throws ReportedException {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerPacketListener#onPacketError");
    }
}
