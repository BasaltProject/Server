// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.ReportedException;

public class FileFixException extends RuntimeException {
    protected final FileSystemCapabilities fileSystemCapabilities;

    public FileFixException(Exception cause, FileSystemCapabilities fileSystemCapabilities) {
        super(cause);
        this.fileSystemCapabilities = fileSystemCapabilities;
    }

    protected CrashReport createCrashReport() {
        CrashReport report = CrashReport.forThrowable(this, "Upgrading world failed with errors");
        CrashReportCategory category = report.addCategory("File system capabilities");
        category.setDetail("Hard Links", fileSystemCapabilities == null ? "null" : fileSystemCapabilities.hardLinks());
        category.setDetail("Atomic Move", fileSystemCapabilities == null ? "null" : fileSystemCapabilities.atomicMove());
        return report;
    }

    public ReportedException makeReportedException() {
        return new ReportedException(createCrashReport());
    }
}
