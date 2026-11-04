// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import net.minecraft.CrashReport;
import net.minecraft.ReportedException;
public class ReportedNbtException extends ReportedException {
    public ReportedNbtException(CrashReport report) {
        super(report);
    }
}
