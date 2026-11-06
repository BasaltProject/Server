// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import com.mojang.datafixers.DataFixerBuilder;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import net.minecraft.util.filefix.virtualfilesystem.FileMove;
import net.minecraft.util.worldupdate.UpgradeProgress;
import net.minecraft.world.level.storage.LevelStorageSource;

/** World files are brought to the current layout by the native world importer, so no layout step is left to run. */
public class FileFixerUpper {
    private final int latestFileFixerVersion;

    public FileFixerUpper(DataFixerBuilder.Result dataFixer, List<FileFix> fileFixes, int latestFileFixerVersion) {
        this.latestFileFixerVersion = latestFileFixerVersion;
    }

    public static int worldVersionToFileFixerVersion(int levelDataVersion) {
        return levelDataVersion;
    }

    public boolean requiresFileFixing(int levelDataVersion) {
        return false;
    }

    public Dynamic<?> fix(LevelStorageSource.LevelStorageAccess worldAccess, Dynamic<?> levelDataTag, UpgradeProgress upgradeProgress) throws FileFixException {
        return fix(worldAccess, levelDataTag, upgradeProgress, latestFileFixerVersion);
    }

    public Dynamic<?> fix(LevelStorageSource.LevelStorageAccess worldAccess, Dynamic<?> levelDataTag, UpgradeProgress upgradeProgress, int toVersion) throws FileFixException {
        return levelDataTag;
    }

    public void applyFileFixers(UpgradeProgress upgradeProgress, int loadedVersion, int toVersion, Path basePath) throws IOException {
    }

    protected static void swapInFixedWorld(LevelStorageSource.LevelStorageAccess worldAccess, List<FileMove> moves, Path fileFixDirectory, Path tempWorld, UpgradeProgress upgradeProgress) throws FileFixException {
    }

    public static FileSystemCapabilities detectFileSystemCapabilities(Path dir) throws IOException {
        Path probe = Files.createTempFile(dir, "capability", ".tmp");
        try {
            Path moved = probe.resolveSibling(probe.getFileName() + ".moved");
            boolean atomic;
            try {
                Files.move(probe, moved, java.nio.file.StandardCopyOption.ATOMIC_MOVE);
                atomic = true;
            } catch (java.nio.file.AtomicMoveNotSupportedException unsupported) {
                atomic = false;
                moved = probe;
            }
            Path link = moved.resolveSibling(moved.getFileName() + ".link");
            boolean hard;
            try {
                Files.createLink(link, moved);
                hard = true;
                Files.deleteIfExists(link);
            } catch (IOException | UnsupportedOperationException unsupported) {
                hard = false;
            }
            Files.deleteIfExists(moved);
            return new FileSystemCapabilities(atomic, hard);
        } finally {
            Files.deleteIfExists(probe);
        }
    }

    public static record UpgradeInProgress(List<FileMove> moves) {
        public static Codec<UpgradeInProgress> codec(Path fromDirectory, Path toDirectory) {
            return com.mojang.serialization.MapCodec.unit(new UpgradeInProgress(new ArrayList<>())).codec();
        }
    }

    public static class Builder {
        public final List<FileFix> fileFixes = new ArrayList<>();
        private final int currentVersion;

        public Builder(int currentVersion) {
            this.currentVersion = currentVersion;
        }

        public void addFixer(FileFix fileFix) {
            fileFixes.add(fileFix);
        }

        public Schema addSchema(DataFixerBuilder fixerUpper, int version, BiFunction<Integer, Schema, Schema> factory) {
            return fixerUpper.addSchema(version, factory);
        }

        public FileFixerUpper build(DataFixerBuilder.Result dataFixer) {
            return new FileFixerUpper(dataFixer, fileFixes, currentVersion);
        }
    }
}
