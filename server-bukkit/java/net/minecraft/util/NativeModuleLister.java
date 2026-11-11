// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.List;
import java.util.Optional;
import net.minecraft.CrashReportCategory;

public class NativeModuleLister {
    public static List<NativeModuleInfo> listModules() {
        return List.of();
    }

    public static Optional<NativeModuleVersion> tryGetModuleVersion(String path) {
        return Optional.empty();
    }

    public static void addCrashSection(CrashReportCategory category) {
    }

    public static record NativeModuleVersion(String description, String version, String company) {
    }

    public static record NativeModuleInfo(String name, Optional<NativeModuleVersion> version) {
    }
}
