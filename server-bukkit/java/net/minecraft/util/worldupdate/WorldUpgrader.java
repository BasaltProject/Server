// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.worldupdate;

import com.mojang.datafixers.DataFixer;
import java.util.Set;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.storage.LevelStorageSource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** World upgrades run natively in Metamorph, so this reports an already finished job. */
public class WorldUpgrader implements AutoCloseable {
    private static final Logger kLogger = LoggerFactory.getLogger(WorldUpgrader.class);
    private final UpgradeProgress progress = new UpgradeProgress();
    private final UpgradeStatusTranslator translator = new UpgradeStatusTranslator();

    public WorldUpgrader(LevelStorageSource.LevelStorageAccess levelSource, DataFixer dataFixer, RegistryAccess registryAccess, boolean eraseCache,
            boolean recreateRegionFiles) {
        progress.setStatus(UpgradeProgress.Status.FINISHED);
        progress.setFinished(true);
    }

    public static CompoundTag getDataFixContextTag(Registry<LevelStem> dimensions, ResourceKey<Level> dimension) {
        return new CompoundTag();
    }

    public static boolean verifyChunkPosAndEraseCache(ChunkPos pos, CompoundTag upgradedTag) {
        verifyChunkPos(pos, upgradedTag);
        boolean changed = upgradedTag.contains("Heightmaps");
        upgradedTag.remove("Heightmaps");
        changed = changed || upgradedTag.contains("isLightOn");
        upgradedTag.remove("isLightOn");
        return changed;
    }

    public static boolean verifyChunkPos(ChunkPos pos, CompoundTag upgradedTag) {
        ChunkPos stored = new ChunkPos(upgradedTag.getIntOr("xPos", 0), upgradedTag.getIntOr("zPos", 0));
        if (!stored.equals(pos)) {
            kLogger.warn("Chunk {} has invalid position {}", pos, stored);
        }
        return false;
    }

    public void cancel() {
        progress.setCanceled();
    }

    public boolean isFinished() {
        return progress.isFinished();
    }

    public Set<ResourceKey<Level>> levels() {
        return Set.of();
    }

    public float dimensionProgress(ResourceKey<Level> dimension) {
        return progress.getDimensionProgress(dimension);
    }

    public float getTotalProgress() {
        return progress.getTotalProgress();
    }

    public int getTotalChunks() {
        return progress.getTotalChunks();
    }

    public int getConverted() {
        return progress.getConverted();
    }

    public int getSkipped() {
        return progress.getSkipped();
    }

    public Component getStatus() {
        return translator.translate(progress);
    }

    @Override
    public void close() {
    }
}
