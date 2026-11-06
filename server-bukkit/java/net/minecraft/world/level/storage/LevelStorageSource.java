// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import java.nio.file.Path;
import net.minecraft.resources.ResourceKey;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.datafixers.DataFixer;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.time.Instant;
import java.time.ZonedDateTime;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.server.WorldLoader;
import net.minecraft.server.packs.repository.PackRepository;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.WorldDataConfiguration;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.levelgen.WorldGenSettings;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
import net.minecraft.world.level.validation.ContentValidationException;
import net.minecraft.world.level.validation.DirectoryValidator;
import org.apache.commons.io.function.IORunnable;
public class LevelStorageSource {
    private Path baseDir;
    private Path backupDir;
    private DirectoryValidator worldDirValidator;
    private DataFixer fixerUpper;

    protected LevelStorageSource() {
    }
    public LevelStorageSource.LevelStorageAccess createAccess(String levelId) {
        try {
            LevelStorageAccess access = new LevelStorageAccess(levelId, baseDir.resolve(levelId));
            access.source = this;
            return access;
        } catch (IOException e) {
            throw new java.io.UncheckedIOException(e);
        }
    }
    public static LevelStorageSource createDefault(Path path) {
        LevelStorageSource source = new LevelStorageSource();
        source.baseDir = path;
        return source;
    }

    public static class LevelStorageAccess implements AutoCloseable {
        private String levelId;
        private Path levelPath;
        private LevelStorageSource source;

        protected LevelStorageAccess() {
        }
        private net.minecraft.nbt.CompoundTag synthesize() {
            net.minecraft.nbt.CompoundTag data = new net.minecraft.nbt.CompoundTag();
            org.bukkit.World world = org.bukkit.Bukkit.getWorld(levelId);
            String displayName = levelId;
            try {
                Path named = levelPath.resolve("name.txt");
                if (java.nio.file.Files.isRegularFile(named)) {
                    displayName = java.nio.file.Files.readString(named).strip();
                }
            } catch (IOException ignored) {
            }
            data.putString("LevelName", displayName);
            data.putInt("DataVersion", org.basaltmc.bukkit.WorldNatives.dataVersion());
            if (world == null) {
                return data;
            }
            org.bukkit.Location spawn = world.getSpawnLocation();
            data.putInt("SpawnX", spawn.getBlockX());
            data.putInt("SpawnY", spawn.getBlockY());
            data.putInt("SpawnZ", spawn.getBlockZ());
            data.putFloat("SpawnAngle", spawn.getYaw());
            data.putLong("Time", world.getGameTime());
            data.putLong("DayTime", world.getFullTime());
            data.putLong("RandomSeed", world.getSeed());
            data.putBoolean("raining", world.hasStorm());
            data.putBoolean("thundering", world.isThundering());
            data.putBoolean("hardcore", world.isHardcore());
            data.putInt("Difficulty", world.getDifficulty().getValue());
            net.minecraft.nbt.CompoundTag rules = new net.minecraft.nbt.CompoundTag();
            String[] ids = org.basaltmc.bukkit.WorldNatives.gameRules(levelId);
            if (ids != null) {
                for (String id : ids) {
                    String found = org.basaltmc.bukkit.WorldNatives.gameRule(levelId, id);
                    if (found != null) {
                        rules.putString(id, found);
                    }
                }
            }
            data.put("GameRules", rules);
            return data;
        }

        private void apply(net.minecraft.nbt.CompoundTag data) {
            org.bukkit.World world = org.bukkit.Bukkit.getWorld(levelId);
            if (world == null) {
                return;
            }
            world.setFullTime(data.getLongOr("DayTime", world.getFullTime()));
            world.setStorm(data.getBooleanOr("raining", world.hasStorm()));
            world.setThundering(data.getBooleanOr("thundering", world.isThundering()));
            world.setSpawnLocation(data.getIntOr("SpawnX", world.getSpawnLocation().getBlockX()),
                data.getIntOr("SpawnY", world.getSpawnLocation().getBlockY()),
                data.getIntOr("SpawnZ", world.getSpawnLocation().getBlockZ()));
            net.minecraft.nbt.CompoundTag rules = data.getCompoundOrEmpty("GameRules");
            for (String id : rules.keySet()) {
                org.basaltmc.bukkit.WorldNatives.setGameRule(levelId, id, rules.getStringOr(id, ""));
            }
        }

        public void close() {

        }
        public Path getDimensionPath(ResourceKey name) {
            return levelPath;
        }
            private LevelStorageAccess(String levelId, Path path) throws IOException {
                this.levelId = levelId;
                this.levelPath = path;
        }
        public void releaseTemporarilyAndRun(IORunnable runnable) throws IOException {
            runnable.run();
        }
        public long estimateDiskSpace() {
            return 0L;
        }
        public boolean checkForLowDiskSpace() {
            return false;
        }
        public void safeClose() {

        }
        public LevelStorageSource parent() {
            return source;
        }
        public LevelDirectory getLevelDirectory() {
            return new LevelDirectory(levelPath);
        }
        public String getLevelId() {
            return levelId;
        }
        private LevelSummary fixAndGetSummaryQuietly() {
            try {
                return fixAndGetSummary();
            } catch (IOException e) {
                return new LevelSummary.CorruptedLevelSummary(levelId, levelPath.resolve("icon.png"), 0L);
            }
        }
        public Path getLevelPath(LevelResource resource) {
            return levelPath.resolve(resource.id());
        }
        public PlayerDataStorage createPlayerStorage() {
        return new PlayerDataStorage(this, source == null ? null : source.fixerUpper);
        }
        public void collectIssues(boolean useFallback) throws IOException {
            return;
        }
        public LevelSummary fixAndGetSummary() throws IOException {
            return fixAndGetSummaryFromTag(getUnfixedDataTag(true));
        }
        public LevelSummary fixAndGetSummaryFromTag(Dynamic<?> dataTag) {
            return source.summaryOf(levelId, levelPath, dataTag.get("Data").orElseEmptyMap());
        }
        public Dynamic<?> getUnfixedDataTagWithFallback() throws IOException {
            return getUnfixedDataTag(true);
        }
        public Dynamic<?> getUnfixedDataTag(boolean useFallback) throws IOException {
            net.minecraft.nbt.CompoundTag root = new net.minecraft.nbt.CompoundTag();
            root.put("Data", synthesize());
            return new Dynamic<>(net.minecraft.nbt.NbtOps.INSTANCE, root);
        }
        public void saveDataTag(WorldData levelData) {
        saveDataTag(levelData, null);
        }
        public void saveDataTag(WorldData levelData, UUID singleplayerUUID) {
        net.minecraft.nbt.CompoundTag root = new net.minecraft.nbt.CompoundTag();
        root.put("Data", levelData.createTag(singleplayerUUID));
        saveLevelData(new Dynamic<>(net.minecraft.nbt.NbtOps.INSTANCE, root));
        }
        public void saveLevelData(Dynamic<?> tag) {
            Object value = tag.getValue();
            if (value instanceof net.minecraft.nbt.CompoundTag root) {
                apply(root.getCompoundOrEmpty("Data"));
            }
        }
        public Optional<Path> getIconFile() {
            Path icon = levelPath.resolve("icon.png");
            return java.nio.file.Files.isRegularFile(icon) ? Optional.of(icon) : Optional.empty();
        }
        public void deleteLevel() throws IOException {
            if (!java.nio.file.Files.exists(levelPath)) {
                return;
            }
            try (java.util.stream.Stream<Path> walk = java.nio.file.Files.walk(levelPath)) {
                for (Path entry : walk.sorted(java.util.Comparator.reverseOrder()).toList()) {
                    java.nio.file.Files.delete(entry);
                }
            }
        }
        private void renameTo(String newName) throws IOException {
            java.nio.file.Files.createDirectories(levelPath);
            java.nio.file.Files.writeString(levelPath.resolve("name.txt"), newName);
        }
        public void renameLevel(String newName) throws IOException {
            renameTo(newName);
        }
        public void renameAndDropPlayer(String newName) throws IOException {
            renameTo(newName);
        }
        public long makeWorldBackup() throws IOException {
            Path backups = (source.baseDir.getParent() == null ? source.baseDir : source.baseDir.getParent()).resolve("backups");
            java.nio.file.Files.createDirectories(backups);
            String stamp = java.time.LocalDateTime.now().format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd_HH-mm-ss"));
            Path archive = backups.resolve(stamp + "_" + levelId + ".zip");
            try (java.util.zip.ZipOutputStream zip = new java.util.zip.ZipOutputStream(java.nio.file.Files.newOutputStream(archive));
                java.util.stream.Stream<Path> walk = java.nio.file.Files.walk(levelPath)) {
                for (Path entry : walk.filter(java.nio.file.Files::isRegularFile).toList()) {
                    zip.putNextEntry(new java.util.zip.ZipEntry(levelId + "/" + levelPath.relativize(entry).toString().replace('\\', '/')));
                    java.nio.file.Files.copy(entry, zip);
                    zip.closeEntry();
                }
            }
            return java.nio.file.Files.size(archive);
        }
        public boolean hasWorldData() {
            return org.bukkit.Bukkit.getWorld(levelId) != null || java.nio.file.Files.exists(levelPath.resolve("level.dat"));
        }
        public boolean restoreLevelDataFromOld() {
            return false;
        }
        public Instant getFileModificationTime(boolean fallback) {
            try {
                return java.nio.file.Files.getLastModifiedTime(levelPath).toInstant();
            } catch (IOException e) {
                return null;
            }
        }
}
    public static final String TAG_DATA = "Data";
    public static final String ALLOWED_SYMLINKS_CONFIG_NAME = "allowed_symlinks.txt";
    public LevelStorageSource(Path baseDir, Path backupDir, DirectoryValidator worldDirValidator, DataFixer fixerUpper) {
        this.baseDir = baseDir;
        this.backupDir = backupDir;
        this.worldDirValidator = worldDirValidator;
        this.fixerUpper = fixerUpper;
    }
    public static DirectoryValidator parseValidator(Path configPath) {
        java.util.List<java.nio.file.PathMatcher> allowed = new java.util.ArrayList<>();
        if (java.nio.file.Files.isRegularFile(configPath)) {
            try {
                for (String line : java.nio.file.Files.readAllLines(configPath)) {
                    String pattern = line.strip();
                    if (!pattern.isEmpty() && !pattern.startsWith("#")) {
                        allowed.add(configPath.getFileSystem().getPathMatcher(pattern.startsWith("regex:") || pattern.startsWith("glob:") ? pattern : "glob:" + pattern));
                    }
                }
            } catch (IOException e) {
                throw new java.io.UncheckedIOException(e);
            }
        }
        return new DirectoryValidator(path -> allowed.stream().anyMatch(matcher -> matcher.matches(path)));
    }
    public static WorldDataConfiguration readDataConfig(Dynamic<?> levelData) {
        return WorldDataConfiguration.CODEC.parse(levelData).resultOrPartial(message -> { }).orElse(WorldDataConfiguration.DEFAULT);
    }
    public static WorldLoader.PackConfig getPackConfig(Dynamic<?> levelDataTag, PackRepository packRepository, boolean safeMode) {
        return new WorldLoader.PackConfig(packRepository, readDataConfig(levelDataTag), safeMode, false);
    }
    public static LevelDataAndDimensions getLevelDataAndDimensions(LevelStorageAccess worldAccess, Dynamic<?> levelDataTag, WorldDataConfiguration dataConfiguration, Registry<LevelStem> datapackDimensions, HolderLookup.Provider registryAccess) {
        net.minecraft.world.level.levelgen.WorldGenSettings settings = new WorldGenSettings(
            net.minecraft.world.level.levelgen.WorldOptions.defaultWithRandomSeed(), new net.minecraft.world.level.levelgen.WorldDimensions(datapackDimensions));
        net.minecraft.world.level.levelgen.WorldDimensions.Complete dimensions = settings.dimensions().bake(datapackDimensions);
        return LevelDataAndDimensions.create(new BasaltWorldData(), settings, dimensions);
    }
    public static <T extends SavedData> DataResult<T> readExistingSavedData(LevelStorageAccess access, HolderLookup.Provider registryAccess, SavedDataType<T> savedDataType) {
        Path location = savedDataType.id().withSuffix(".dat").resolveAgainst(access.getLevelPath(LevelResource.DATA));
        net.minecraft.nbt.CompoundTag contents;
        try {
            contents = net.minecraft.nbt.NbtIo.readCompressed(location, net.minecraft.nbt.NbtAccounter.unlimitedHeap());
        } catch (IOException e) {
            return DataResult.error(e::getMessage);
        }
        return savedDataType.codec().parse(net.minecraft.resources.RegistryOps.create(net.minecraft.nbt.NbtOps.INSTANCE, registryAccess),
            contents.getCompoundOrEmpty("data"));
    }
    public static void writeGameRules(WorldData worldData, Path worldFolder, GameRules gameRules) throws IOException {
        writeSavedData(worldFolder, net.minecraft.nbt.NbtOps.INSTANCE, "game_rules", GameRules.codec(worldData.enabledFeatures()), gameRules);
    }
    public static void writeWorldGenSettings(RegistryAccess registryAccess, Path worldFolder, WorldGenSettings worldGenSettings) throws IOException {
        writeSavedData(worldFolder, net.minecraft.resources.RegistryOps.create(net.minecraft.nbt.NbtOps.INSTANCE, (HolderLookup.Provider) registryAccess),
            "world_gen_settings", WorldGenSettings.CODEC, worldGenSettings);
    }
    private static <T> void writeSavedData(Path worldFolder, com.mojang.serialization.DynamicOps<net.minecraft.nbt.Tag> ops, String name, com.mojang.serialization.Codec<T> codec, T data) throws IOException {
        net.minecraft.nbt.CompoundTag full = new net.minecraft.nbt.CompoundTag();
        full.put("data", codec.encodeStart(ops, data).getOrThrow());
        full.putInt("DataVersion", org.basaltmc.bukkit.WorldNatives.dataVersion());
        Path path = worldFolder.resolve("data").resolve("minecraft").resolve(name + ".dat");
        java.nio.file.Files.createDirectories(path.getParent());
        net.minecraft.nbt.NbtIo.writeCompressed(full, path);
    }
    public String getName() {
        return "Anvil";
    }
    public LevelCandidates findLevelCandidates() throws LevelStorageException {
        List<LevelDirectory> found = new java.util.ArrayList<>();
        if (java.nio.file.Files.isDirectory(baseDir)) {
            try (java.util.stream.Stream<Path> entries = java.nio.file.Files.list(baseDir)) {
                entries.filter(java.nio.file.Files::isDirectory).sorted().forEach(entry -> found.add(new LevelDirectory(entry)));
            } catch (IOException e) {
                throw new LevelStorageException(net.minecraft.network.chat.Component.translatable("selectWorld.load_folder_access"));
            }
        }
        return new LevelCandidates(found);
    }
    public CompletableFuture<List<LevelSummary>> loadLevelSummaries(LevelCandidates candidates) {
        List<LevelSummary> summaries = new java.util.ArrayList<>();
        for (LevelDirectory directory : candidates) {
            String id = directory.directoryName();
            if (java.nio.file.Files.isSymbolicLink(directory.path())) {
                summaries.add(new LevelSummary.SymlinkLevelSummary(id, directory.iconFile()));
                continue;
            }
            summaries.add(createAccess(id).fixAndGetSummaryQuietly());
        }
        java.util.Collections.sort(summaries);
        return CompletableFuture.completedFuture(summaries);
    }
    private LevelSummary summaryOf(String levelId, Path levelPath, Dynamic<?> data) {
        net.minecraft.world.level.LevelSettings settings = net.minecraft.world.level.LevelSettings.parse(data, null);
        LevelVersion version = LevelVersion.parse(data);
        Path icon = levelPath.resolve("icon.png");
        return new LevelSummary(settings, version, levelId, false, false, false, false, icon);
    }
    public boolean isNewLevelIdAcceptable(String levelId) {
        return !levelId.isBlank() && !levelId.contains("/") && !levelId.contains("\\") && !java.nio.file.Files.exists(getLevelPath(levelId));
    }
    public boolean levelExists(String levelId) {
        return java.nio.file.Files.isDirectory(getLevelPath(levelId));
    }
    public Path getLevelPath(String levelId) {
        return baseDir.resolve(levelId);
    }
    public Path getBaseDir() {
        return baseDir;
    }
    public Path getBackupPath() {
        return baseDir.resolveSibling("backups");
    }
    public LevelStorageAccess validateAndCreateAccess(String levelId) throws IOException, ContentValidationException {
        return createAccess(levelId);
    }
    public DirectoryValidator getWorldDirValidator() {
        return worldDirValidator;
    }
    public static record LevelCandidates(List<LevelDirectory> levels) implements Iterable<LevelDirectory> {
        public boolean isEmpty() {
            return levels.isEmpty();
        }
        public Iterator<LevelDirectory> iterator() {
            return levels.iterator();
        }
    }
    public static record LevelDirectory(Path path) {
        public String directoryName() {
            return path.getFileName().toString();
        }
        public Path dataFile() {
            return path.resolve("level.dat");
        }
        public Path oldDataFile() {
            return path.resolve("level.dat_old");
        }
        public Path corruptedDataFile(ZonedDateTime time) {
            return path.resolve("level.dat_corrupted_" + time.toEpochSecond());
        }
        public Path rawDataFile(ZonedDateTime time) {
            return path.resolve("level.dat_raw_" + time.toEpochSecond());
        }
        public Path iconFile() {
            return path.resolve("icon.png");
        }
        public Path lockFile() {
            return path.resolve("session.lock");
        }
        public Path resourcePath(LevelResource resource) {
            return path.resolve(resource.id());
        }
    }
}
