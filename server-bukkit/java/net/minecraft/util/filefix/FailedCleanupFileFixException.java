// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;

public final class FailedCleanupFileFixException extends FileFixException {
    private final String newWorldFolderName;

    public FailedCleanupFileFixException(Exception cause, String newWorldFolderName,
            FileSystemCapabilities fileSystemCapabilities) {
        super(cause, fileSystemCapabilities);
        this.newWorldFolderName = newWorldFolderName;
    }

    @Override
    protected CrashReport createCrashReport() {
        CrashReport report = super.createCrashReport();
        report.addCategory("World upgrade").setDetail("New Name", newWorldFolderName);
        return report;
    }

    public String newWorldFolderName() {
        return newWorldFolderName;
    }
}
