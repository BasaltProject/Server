// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;
import net.minecraft.CrashReport;
import net.minecraft.ReportedException;
import net.minecraft.network.PacketListener;
import net.minecraft.network.PacketProcessor;
import net.minecraft.server.RunningOnDifferentThreadException;
import net.minecraft.server.level.ServerLevel;
public class PacketUtils {
    public static <T extends PacketListener> void ensureRunningOnSameThread(Packet<T> packet, T listener, ServerLevel level) throws RunningOnDifferentThreadException {
        net.minecraft.server.MinecraftServer server = level.getServer();
        if (!server.isSameThread()) {
            server.executeIfPossible(() -> packet.handle(listener));
            throw RunningOnDifferentThreadException.RUNNING_ON_DIFFERENT_THREAD;
        }
    }
    public static <T extends PacketListener> void ensureRunningOnSameThread(Packet<T> packet, T listener, PacketProcessor packetProcessor) throws RunningOnDifferentThreadException {
        if (!packetProcessor.isSameThread()) {
            packetProcessor.scheduleIfPossible(listener, packet);
            throw RunningOnDifferentThreadException.RUNNING_ON_DIFFERENT_THREAD;
        }
    }
    public static <T extends PacketListener> ReportedException makeReportedException(Exception cause, Packet<T> packet, T listener) {
        CrashReport report = CrashReport.forThrowable(cause, "Main thread packet handler");
        return new ReportedException(report);
    }
    public static <T extends PacketListener> void fillCrashReport(CrashReport report, T listener, Packet<T> packet) {
    }
}
