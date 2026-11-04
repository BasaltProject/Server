// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.ReportedException;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketFlow;
public interface PacketListener {
    default PacketFlow flow() {
        return PacketFlow.SERVERBOUND;
    }
    default ConnectionProtocol protocol() {
        return ConnectionProtocol.PLAY;
    }
    default void onDisconnect(DisconnectionDetails details) {

    }
    default void onPacketError(Packet packet, Exception cause) throws ReportedException {
        throw new IllegalStateException("Packet error", cause);
    }
    default DisconnectionDetails createDisconnectionInfo(Component reason, Throwable cause) {
        return new DisconnectionDetails(reason);
    }
    default boolean isAcceptingMessages() {
        return true;
    }
    default boolean shouldHandleMessage(Packet<?> packet) {
        return isAcceptingMessages();
    }
    default void fillCrashReport(CrashReport crashReport) {

    }
    default void fillListenerSpecificCrashDetails(CrashReport report, CrashReportCategory connectionDetails) {

    }
}
