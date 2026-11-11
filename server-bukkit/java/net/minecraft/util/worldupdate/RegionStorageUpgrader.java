// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.worldupdate;

import com.mojang.datafixers.DataFixer;
import it.unimi.dsi.fastutil.ints.Int2ObjectMap;
import it.unimi.dsi.fastutil.ints.Int2ObjectOpenHashMap;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;
import net.minecraft.world.level.chunk.storage.SimpleRegionStorage;
import net.minecraft.world.level.storage.LevelStorageSource;

/** Region upgrades run natively in Metamorph; this holds the configuration and reports no work. */
public class RegionStorageUpgrader {
    protected CompletableFuture<Void> previousWriteFuture = CompletableFuture.completedFuture(null);
    protected final DataFixTypes dataFixType;
    protected final int defaultVersion;
    private final UpgradeProgress progress;

    protected RegionStorageUpgrader(DataFixer dataFixer, DataFixTypes dataFixType, String type, String folderName, int defaultVersion,
            boolean recreateRegionFiles, UpgradeProgress upgradeProgress, int startIndex, CompoundTag dataFixContextTag,
            Int2ObjectMap<TagModifier> tagModifiers) {
        this.dataFixType = dataFixType;
        this.defaultVersion = defaultVersion;
        this.progress = upgradeProgress;
    }

    public void init(ResourceKey<Level> dimensionKey, LevelStorageSource.LevelStorageAccess levelStorage) {
    }

    public void upgrade() {
        progress.setStatus(UpgradeProgress.Status.FINISHED);
    }

    protected final SimpleRegionStorage createStorage(RegionStorageInfo info, Path regionFolder) {
        throw new UnsupportedOperationException("Region storage upgrades are performed by Metamorph");
    }

    public int fileAmount() {
        return 0;
    }

    protected boolean tryProcessOnePosition(SimpleRegionStorage storage, ChunkPos pos) {
        return false;
    }

    protected CompoundTag upgradeTag(SimpleRegionStorage storage, CompoundTag chunkTag, int targetVersion) {
        return chunkTag;
    }

    protected static Path resolveRecreateDirectory(Path directoryPath) {
        return directoryPath.resolveSibling(directoryPath.getFileName() + "_new");
    }

    public static class Builder {
        private final DataFixer dataFixer;
        private DataFixTypes dataFixType;
        private String type;
        private String folderName;
        private int defaultVersion;
        private boolean recreateRegionFiles;
        private UpgradeProgress progress = new UpgradeProgress.Noop();
        private CompoundTag contextTag = new CompoundTag();
        private final Int2ObjectMap<TagModifier> tagModifiers = new Int2ObjectOpenHashMap<>();

        public Builder(DataFixer dataFixer) {
            this.dataFixer = dataFixer;
        }

        public Builder setDataFixType(DataFixTypes dataFixType) {
            this.dataFixType = dataFixType;
            return this;
        }

        public Builder setTypeAndFolderName(String name) {
            return setType(name).setFolderName(name);
        }

        public Builder setType(String type) {
            this.type = type;
            return this;
        }

        public Builder setFolderName(String folderName) {
            this.folderName = folderName;
            return this;
        }

        public Builder setDefaultVersion(int defaultVersion) {
            this.defaultVersion = defaultVersion;
            return this;
        }

        public Builder setRecreateRegionFiles(boolean recreateRegionFiles) {
            this.recreateRegionFiles = recreateRegionFiles;
            return this;
        }

        public Builder trackProgress(UpgradeProgress upgradeProgress) {
            this.progress = upgradeProgress;
            return this;
        }

        public Builder setDataFixContextTag(CompoundTag dataFixContextTag) {
            this.contextTag = dataFixContextTag;
            return this;
        }

        public Builder addTagModifier(int version, TagModifier tagModifier) {
            tagModifiers.put(version, tagModifier);
            return this;
        }

        public Builder copy() {
            Builder copy = new Builder(dataFixer).setDataFixType(dataFixType).setType(type).setFolderName(folderName).setDefaultVersion(defaultVersion)
                .setRecreateRegionFiles(recreateRegionFiles).trackProgress(progress).setDataFixContextTag(contextTag);
            copy.tagModifiers.putAll(tagModifiers);
            return copy;
        }

        public RegionStorageUpgrader build(int previousCopiesFileAmounts) {
            return new RegionStorageUpgrader(dataFixer, dataFixType, type, folderName, defaultVersion, recreateRegionFiles, progress,
                previousCopiesFileAmounts, contextTag, tagModifiers);
        }
    }

    public interface TagModifier {
        boolean modifyTagAfterFix(ChunkPos pos, CompoundTag upgradedTag);
    }
}
