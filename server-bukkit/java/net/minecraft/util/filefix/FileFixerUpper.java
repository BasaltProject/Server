// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.mojang.datafixers.DataFixerBuilder;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.JsonOps;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.CopyOption;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.BiFunction;
import net.minecraft.SharedConstants;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.util.FileUtil;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.util.datafix.DataFixers;
import net.minecraft.util.filefix.virtualfilesystem.CopyOnWriteFileSystem;
import net.minecraft.util.filefix.virtualfilesystem.FileMove;
import net.minecraft.util.worldupdate.UpgradeProgress;
import net.minecraft.world.level.storage.LevelResource;
import net.minecraft.world.level.storage.LevelStorageSource;
import net.minecraft.world.level.storage.PrimaryLevelData;
import org.basaltmc.bukkit.FileNatives;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class FileFixerUpper {
    private static final int FILE_FIXER_INTRODUCTION_VERSION = 4772;
    private static final Gson GSON = new Gson().newBuilder().setPrettyPrinting().create();
    private static final String FILE_FIX_DIRECTORY_NAME = "filefix";
    private static final String NEW_WORLD_TEMP_NAME = "new_world";
    private static final String UPGRADE_IN_PROGRESS_NAME = "upgrade_in_progress.json";
    private static final Logger LOGGER = LoggerFactory.getLogger(FileFixerUpper.class);
    private final DataFixerBuilder.Result dataFixer;
    private final List<FileFix> fileFixes;
    private final int latestFileFixerVersion;

    public FileFixerUpper(DataFixerBuilder.Result dataFixer, List<FileFix> fileFixes, int latestVersion) {
        this.dataFixer = dataFixer;
        this.fileFixes = fileFixes.stream().toList();
        this.latestFileFixerVersion = latestVersion;
    }

    public static int worldVersionToFileFixerVersion(int levelDataVersion) {
        return levelDataVersion < FILE_FIXER_INTRODUCTION_VERSION ? 0 : levelDataVersion;
    }

    public boolean requiresFileFixing(int levelDataVersion) {
        return worldVersionToFileFixerVersion(levelDataVersion) < latestFileFixerVersion
            && !applicableFixers(levelDataVersion, latestFileFixerVersion).isEmpty();
    }

    private com.mojang.datafixers.DataFixer fixer() {
        return dataFixer != null ? dataFixer.fixer() : DataFixers.getDataFixer();
    }

    public Dynamic<?> fix(LevelStorageSource.LevelStorageAccess worldAccess, Dynamic<?> levelDataTag,
            UpgradeProgress progress) throws FileFixException {
        return fix(worldAccess, levelDataTag, progress, SharedConstants.getCurrentVersion().dataVersion().version());
    }

    public Dynamic<?> fix(LevelStorageSource.LevelStorageAccess worldAccess, Dynamic<?> levelDataTag,
            UpgradeProgress progress, int toVersion) throws FileFixException {
        int storedVersion = NbtUtils.getDataVersion(levelDataTag);
        if (requiresFileFixing(storedVersion)) {
            LOGGER.info("Starting upgrade for world \"{}\"", worldAccess.getLevelId());
            progress.notifications().worldUpgradeStarted();
            Path worldDirectory = worldAccess.getLevelDirectory().path();
            Path fileFixDirectory = worldDirectory.resolve(FILE_FIX_DIRECTORY_NAME);
            Path tempWorld = fileFixDirectory.resolve(NEW_WORLD_TEMP_NAME);
            List<FileMove> moves;
            try {
                moves = startOrContinue(progress, toVersion, worldDirectory, tempWorld, fileFixDirectory, storedVersion);
            } catch (IOException failure) {
                progress.notifications().worldUpgradeFailed("The world upgrade failed during file fixing");
                throw new AbortedFileFixException(failure);
            }
            try {
                swapInFixedWorld(worldAccess, moves, fileFixDirectory, tempWorld, progress);
            } catch (AbortedFileFixException failure) {
                if (failure.notRevertedMoves().isEmpty()) {
                    cleanup(fileFixDirectory);
                    progress.notifications().worldUpgradeFailed("The world upgrade failed while swapping in the file-fixed world");
                } else {
                    progress.notifications().worldUpgradeFailed(
                        "The world upgrade failed while swapping in the file-fixed world and some moves were not reverted");
                }
                throw failure;
            }
            try {
                Dynamic<?> reread = worldAccess.getUnfixedDataTag(false);
                levelDataTag = reread;
            } catch (IOException failure) {
                progress.notifications().worldUpgradeFailed("The world upgrade failed while reading level.dat");
                throw new UncheckedIOException(failure);
            }
            storedVersion = NbtUtils.getDataVersion(levelDataTag);
        }
        Dynamic<?> fixedLevel = DataFixTypes.LEVEL.updateToCurrentVersion(fixer(), levelDataTag, storedVersion);
        return stampVersions(fixedLevel, toVersion);
    }

    private List<FileMove> startOrContinue(UpgradeProgress progress, int toVersion, Path worldDirectory, Path tempWorld,
            Path fileFixDirectory, int storedVersion) throws IOException {
        Path inProgress = fileFixDirectory.resolve(UPGRADE_IN_PROGRESS_NAME);
        if (FileNatives.stat(inProgress.toString()) != null) {
            LOGGER.warn("A world upgrade was interrupted earlier, resuming it");
            return readMoves(worldDirectory, tempWorld, inProgress);
        }
        if (FileNatives.stat(fileFixDirectory.toString()) != null) {
            FileNatives.removeTree(fileFixDirectory.toString());
        }
        try {
            if (!FileNatives.makeDirectories(fileFixDirectory.toString())) {
                throw new IOException("Cannot create " + fileFixDirectory);
            }
            return applyOnCow(progress, storedVersion, toVersion, worldDirectory, fileFixDirectory, tempWorld);
        } catch (IOException | RuntimeException failure) {
            cleanup(fileFixDirectory);
            throw failure;
        }
    }

    private static void cleanup(Path fileFixDirectory) {
        if (!FileNatives.removeTree(fileFixDirectory.toString())) {
            LOGGER.error("Failed to clean up {}", fileFixDirectory);
        }
    }

    private List<FileMove> applyOnCow(UpgradeProgress progress, int storedVersion, int toVersion, Path worldDirectory,
            Path fileFixDirectory, Path tempWorld) throws IOException {
        try (CopyOnWriteFileSystem fs = CopyOnWriteFileSystem.create(worldDirectory.getFileName().toString(), worldDirectory,
                fileFixDirectory.resolve("cow"), fileFixDirectory::equals)) {
            applyFileFixers(progress, storedVersion, toVersion, fs.rootPath());
            CopyOnWriteFileSystem.Moves plan = fs.collectMoveOperations(tempWorld);
            if (!FileNatives.makeDirectories(tempWorld.toString())) {
                throw new IOException("Cannot create " + tempWorld);
            }
            CopyOnWriteFileSystem.createDirectories(plan.directories());
            CopyOnWriteFileSystem.moveFiles(plan.copiedFiles());
            return plan.preexistingFiles();
        }
    }

    public void applyFileFixers(UpgradeProgress progress, int storedVersion, int toVersion, Path basePath)
            throws IOException {
        List<FileFix> applicable = applicableFixers(storedVersion, toVersion);
        progress.setType(UpgradeProgress.Type.FILES);
        progress.setStatus(UpgradeProgress.Status.COUNTING);
        int total = 0;
        for (FileFix fix : applicable) {
            total += fix.countFileOperations();
        }
        progress.addTotalFileFixOperations(total);
        progress.setStatus(UpgradeProgress.Status.UPGRADING);
        progress.setApplicableFixerAmount(applicable.size());
        for (FileFix fix : applicable) {
            progress.incrementRunningFileFixer();
            fix.runFixOperations(basePath, progress);
        }
        writeUpdatedLevelData(basePath, toVersion);
        java.nio.file.Files.deleteIfExists(basePath.resolve("level.dat_old"));
        java.nio.file.Files.deleteIfExists(basePath.resolve("session.lock"));
    }

    private List<FileFix> applicableFixers(int fromVersion, int toVersion) {
        int from = worldVersionToFileFixerVersion(fromVersion);
        return fileFixes.stream().filter(fix -> fix.getVersion() > from && fix.getVersion() <= toVersion).toList();
    }

    private void writeUpdatedLevelData(Path worldDirectory, int toVersion) throws IOException {
        Path levelDat = worldDirectory.resolve(LevelResource.LEVEL_DATA_FILE.id());
        CompoundTag file = NbtIo.readCompressed(levelDat, NbtAccounter.defaultQuota());
        CompoundTag data = file.getCompoundOrEmpty("Data");
        Dynamic<?> fixed = DataFixTypes.LEVEL.update(fixer(), new Dynamic<>(NbtOps.INSTANCE, data),
            NbtUtils.getDataVersion(data), toVersion);
        fixed = stampVersions(fixed, toVersion);
        NbtIo.writeCompressed((CompoundTag) fixed.emptyMap().set("Data", fixed).convert(NbtOps.INSTANCE).getValue(),
            levelDat);
    }

    private static Dynamic<?> stampVersions(Dynamic<?> fixed, int toVersion) {
        return PrimaryLevelData.writeVersionTag(PrimaryLevelData.writeLastPlayed(NbtUtils.addDataVersion(fixed, toVersion)));
    }

    protected static void swapInFixedWorld(LevelStorageSource.LevelStorageAccess worldAccess, List<FileMove> moves,
            Path fileFixDirectory, Path tempWorld, UpgradeProgress progress) throws FileFixException {
        Path worldDirectory = worldAccess.getLevelDirectory().path();
        Path saves = worldDirectory.getParent();
        String worldName = String.valueOf(worldDirectory.getFileName());
        FileSystemCapabilities capabilities;
        Path upgradedTopLevel;
        Path outdatedFolder;
        try {
            capabilities = detectFileSystemCapabilities(fileFixDirectory);
            upgradedTopLevel = saves.resolve(FileUtil.findAvailableName(saves, worldName + " upgraded", ""));
            outdatedFolder = saves.resolve(FileUtil.findAvailableName(saves, worldName + " OUTDATED", ""));
        } catch (Exception failure) {
            throw new AbortedFileFixException(failure);
        }
        if (!capabilities.atomicMove()) {
            throw new AtomicMoveNotSupportedFileFixException(capabilities);
        }
        CopyOption[] moveOptions = capabilities.getMoveOptions();
        LOGGER.info("File system capabilities: {}", capabilities);
        Path movesFile = fileFixDirectory.resolve(UPGRADE_IN_PROGRESS_NAME);
        try {
            if (capabilities.hardLinks()) {
                CopyOnWriteFileSystem.hardLinkFiles(moves);
            } else {
                writeMoves(moves, worldDirectory, tempWorld, movesFile);
            }
        } catch (Exception failure) {
            throw new AbortedFileFixException(failure, List.of(), capabilities);
        }
        LOGGER.info("Applying file structure changes for world \"{}\"", worldAccess.getLevelId());
        if (capabilities.hardLinks()) {
            if (!FileNatives.move(tempWorld.toString(), upgradedTopLevel.toString(), true)) {
                throw new AbortedFileFixException(new IOException("Cannot move " + tempWorld), List.of(), capabilities);
            }
        } else {
            try {
                LOGGER.info("Relocating files into the new layout");
                CopyOnWriteFileSystem.moveFilesWithRetry(moves, moveOptions);
                if (!FileNatives.move(tempWorld.toString(), upgradedTopLevel.toString(), true)) {
                    throw new IOException("Cannot move " + tempWorld);
                }
            } catch (Exception failure) {
                LOGGER.error("Encountered error while trying to create new world folder:", failure);
                List<FileMove> failed = CopyOnWriteFileSystem.tryRevertMoves(moves, moveOptions);
                if (failed.isEmpty()) {
                    FileNatives.remove(movesFile.toString());
                }
                throw new AbortedFileFixException(failure, failed, capabilities);
            }
            FileNatives.remove(movesFile.toString());
        }
        LOGGER.info("Start cleanup");
        FileNatives.remove(worldDirectory.resolve("level.dat").toString());
        FileNatives.remove(worldDirectory.resolve("level.dat_old").toString());
        boolean[] succeeded = {false};
        try {
            Path newFolder = upgradedTopLevel;
            worldAccess.releaseTemporarilyAndRun(() -> {
                LOGGER.info("Setting the outdated world aside");
                if (!FileNatives.move(worldDirectory.toString(), outdatedFolder.toString(), true)
                    && !FileNatives.removeTree(worldDirectory.toString())) {
                    throw new FailedCleanupFileFixException(new IOException("Cannot move out " + worldDirectory),
                        newFolder.getFileName().toString(), capabilities);
                }
                LOGGER.info("Putting the upgraded world in place");
                if (!FileNatives.move(newFolder.toString(), worldDirectory.toString(), true)) {
                    throw new FailedCleanupFileFixException(new IOException("Cannot move in " + newFolder),
                        newFolder.getFileName().toString(), capabilities);
                }
                succeeded[0] = true;
            });
        } catch (IOException failure) {
            Path newWorld = succeeded[0] ? worldDirectory : upgradedTopLevel;
            throw new FailedCleanupFileFixException(failure, newWorld.getFileName().toString(), capabilities);
        }
        LOGGER.info("Done applying file structure changes for world \"{}\". Cleaning up outdated data...",
            worldAccess.getLevelId());
        if (FileNatives.stat(outdatedFolder.toString()) != null && !FileNatives.removeTree(outdatedFolder.toString())) {
            LOGGER.warn("Failed to clean up old world folder");
        }
        LOGGER.info("Upgrade done for world \"{}\"", worldAccess.getLevelId());
        progress.notifications().worldUpgradeFinished();
    }

    private static void writeMoves(List<FileMove> moves, Path fromDirectory, Path toDirectory, Path file)
            throws IOException {
        JsonElement json = UpgradeInProgress.codec(fromDirectory, toDirectory)
            .encodeStart(JsonOps.INSTANCE, new UpgradeInProgress(moves)).getOrThrow();
        if (!FileNatives.write(file.toString(), GSON.toJson(json).getBytes(StandardCharsets.UTF_8))) {
            throw new IOException("Cannot write " + file);
        }
    }

    private static List<FileMove> readMoves(Path fromDirectory, Path toDirectory, Path file) throws IOException {
        byte[] bytes = FileNatives.read(file.toString());
        if (bytes == null) {
            throw new IOException("Cannot read " + file);
        }
        JsonElement json = GSON.fromJson(new String(bytes, StandardCharsets.UTF_8), JsonElement.class);
        return UpgradeInProgress.codec(fromDirectory, toDirectory).decode(JsonOps.INSTANCE, json).getOrThrow()
            .getFirst().moves;
    }

    public static FileSystemCapabilities detectFileSystemCapabilities(Path dir) throws IOException {
        int bits = FileNatives.capabilities(dir.toString());
        return new FileSystemCapabilities((bits & 1) != 0, (bits & 2) != 0);
    }

    public static record UpgradeInProgress(List<FileMove> moves) {
        public static Codec<UpgradeInProgress> codec(Path fromDirectory, Path toDirectory) {
            return RecordCodecBuilder.create(instance -> instance.group(
                FileMove.moveCodec(fromDirectory, toDirectory).listOf().fieldOf("moves").forGetter(UpgradeInProgress::moves)
            ).apply(instance, UpgradeInProgress::new));
        }
    }

    public static class Builder {
        public final List<FileFix> fileFixes = new ArrayList<>();
        private final int currentVersion;
        private int latestFileFixerVersion;
        private final List<Schema> knownSchemas = new ArrayList<>();

        public Builder(int currentVersion) {
            this.currentVersion = currentVersion;
        }

        public void addFixer(FileFix fileFix) {
            if (!knownSchemas.contains(fileFix.getSchema())) {
                throw new IllegalArgumentException(
                    "Tried to add file fixer with unknown schema. Add it through FileFixerUpper#addSchema instead");
            }
            int version = fileFix.getVersion();
            if (version > currentVersion) {
                throw new IllegalArgumentException(String.format(Locale.ROOT,
                    "Tried to add too recent file fix for version: %s. The data version of the game is: %s", version,
                    currentVersion));
            }
            if (!fileFixes.isEmpty() && fileFixes.getLast().getVersion() > version) {
                throw new IllegalArgumentException(String.format(Locale.ROOT,
                    "Tried to add too recent file fix for version: %s. The most recent file fix version is %s",
                    version, fileFixes.getLast().getVersion()));
            }
            fileFixes.add(fileFix);
        }

        public Schema addSchema(DataFixerBuilder fixerUpper, int version, BiFunction<Integer, Schema, Schema> factory) {
            latestFileFixerVersion = Math.max(version, latestFileFixerVersion);
            Schema added = fixerUpper.addSchema(version, factory);
            knownSchemas.add(added);
            return added;
        }

        public FileFixerUpper build(DataFixerBuilder.Result dataFixer) {
            return new FileFixerUpper(dataFixer, fileFixes, latestFileFixerVersion);
        }
    }
}
