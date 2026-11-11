// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.worldupdate;

import it.unimi.dsi.fastutil.objects.Reference2FloatMap;
import it.unimi.dsi.fastutil.objects.Reference2FloatMaps;
import it.unimi.dsi.fastutil.objects.Reference2FloatOpenHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.notifications.EmptyNotificationService;
import net.minecraft.server.notifications.NotificationService;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.Level;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class UpgradeProgress {
    private static final Logger LOGGER = LoggerFactory.getLogger(UpgradeProgress.class);
    private volatile boolean finished;
    private final FileFixStats totalFileFixStats = new FileFixStats();
    private final FileFixStats typeFileFixStats = new FileFixStats();
    private final FileFixStats runningFileFixerStats = new FileFixStats();
    private volatile float totalProgress;
    private final AtomicInteger totalChunks = new AtomicInteger();
    private final AtomicInteger converted = new AtomicInteger();
    private final AtomicInteger skipped = new AtomicInteger();
    private final Reference2FloatMap<ResourceKey<Level>> progressMap =
        Reference2FloatMaps.synchronize(new Reference2FloatOpenHashMap<>());
    private volatile boolean canceled;
    private volatile DataFixTypes dataFixType;
    private volatile Status status = Status.COUNTING;
    private volatile Type type;
    private final AtomicLong lastLoggedProgressTime = new AtomicLong();
    private final NotificationService notificationService;

    public UpgradeProgress() {
        this(new EmptyNotificationService());
    }

    public UpgradeProgress(NotificationService notificationService) {
        this.notificationService = notificationService;
    }

    public boolean isFinished() {
        return finished;
    }

    public void setFinished(boolean finished) {
        this.finished = finished;
    }

    public FileFixStats getTotalFileFixStats() {
        return totalFileFixStats;
    }

    public FileFixStats getTypeFileFixStats() {
        return typeFileFixStats;
    }

    public FileFixStats getRunningFileFixerStats() {
        return runningFileFixerStats;
    }

    public void addTotalFileFixOperations(int additional) {
        totalFileFixStats.totalOperations.addAndGet(additional);
        typeFileFixStats.totalOperations.addAndGet(additional);
    }

    public float getTotalProgress() {
        return totalProgress;
    }

    public void setTotalProgress(float totalProgress) {
        this.totalProgress = totalProgress;
    }

    public int getTotalChunks() {
        return totalChunks.get();
    }

    public void addTotalChunks(int additional) {
        totalChunks.addAndGet(additional);
    }

    public int getConverted() {
        return converted.get();
    }

    public void setDimensionProgress(ResourceKey<Level> dimension, float progress) {
        progressMap.put(dimension, progress);
    }

    public float getDimensionProgress(ResourceKey<Level> dimension) {
        return progressMap.getFloat(dimension);
    }

    public void incrementConverted() {
        converted.incrementAndGet();
    }

    public int getSkipped() {
        return skipped.get();
    }

    public void incrementSkipped() {
        skipped.incrementAndGet();
    }

    public void incrementFinishedOperations() {
        incrementFinishedOperationsBy(1);
    }

    public void incrementFinishedOperationsBy(int count) {
        totalFileFixStats.finishedOperations.addAndGet(count);
        typeFileFixStats.finishedOperations.addAndGet(count);
        logProgress();
    }

    public void setCanceled() {
        canceled = true;
    }

    public boolean isCanceled() {
        return canceled;
    }

    public Status getStatus() {
        return status;
    }

    public void setStatus(Status status) {
        this.status = status;
    }

    public DataFixTypes getDataFixType() {
        return dataFixType;
    }

    public void setType(Type type) {
        this.type = type;
        typeFileFixStats.reset();
    }

    public Type getType() {
        return type;
    }

    public void setApplicableFixerAmount(int amount) {
        runningFileFixerStats.totalOperations.set(amount);
    }

    public void incrementRunningFileFixer() {
        runningFileFixerStats.finishedOperations.incrementAndGet();
    }

    public void reset(DataFixTypes dataFixType) {
        totalFileFixStats.reset();
        typeFileFixStats.reset();
        totalChunks.set(0);
        converted.set(0);
        skipped.set(0);
        this.dataFixType = dataFixType;
    }

    public void logProgress() {
        long now = System.currentTimeMillis();
        if (now >= lastLoggedProgressTime.get() + 1000L) {
            float progress = (float) totalFileFixStats.finishedOperations() / (float) totalFileFixStats.totalOperations();
            lastLoggedProgressTime.set(now);
            LOGGER.info("Upgrading progress: {}%", (int) (progress * 100.0F));
            notificationService.worldUpgradeProgress(progress);
        }
    }

    public NotificationService notifications() {
        return notificationService;
    }

    public enum Status {
        COUNTING, FAILED, FINISHED, UPGRADING
    }

    public enum Type implements StringRepresentable {
        FILES("files"), LEGACY_STRUCTURES("legacy_structures"), REGIONS("regions");

        private final String id;
        private final Component label;

        Type(String id) {
            this.id = id;
            this.label = Component.translatable("upgradeWorld.progress.type." + id);
        }

        @Override
        public String getSerializedName() {
            return id;
        }

        public Component label() {
            return label;
        }
    }

    public static class FileFixStats {
        private final AtomicInteger finishedOperations = new AtomicInteger();
        private final AtomicInteger totalOperations = new AtomicInteger();

        public int finishedOperations() {
            return finishedOperations.get();
        }

        public int totalOperations() {
            return totalOperations.get();
        }

        public void reset() {
            finishedOperations.set(0);
            totalOperations.set(0);
        }

        public float getProgress() {
            int total = totalOperations();
            return total == 0 ? 0.0F : Math.clamp((float) finishedOperations() / (float) total, 0.0F, 1.0F);
        }
    }

    public static class Noop extends UpgradeProgress {
        @Override
        public void setFinished(boolean finished) {}

        @Override
        public void addTotalFileFixOperations(int additional) {}

        @Override
        public void setTotalProgress(float totalProgress) {}

        @Override
        public void addTotalChunks(int additional) {}

        @Override
        public void setDimensionProgress(ResourceKey<Level> dimension, float progress) {}

        @Override
        public void incrementConverted() {}

        @Override
        public void incrementSkipped() {}

        @Override
        public void incrementFinishedOperations() {}

        @Override
        public void incrementFinishedOperationsBy(int count) {}

        @Override
        public void setCanceled() {}

        @Override
        public void setStatus(Status status) {}

        @Override
        public void setApplicableFixerAmount(int amount) {}

        @Override
        public void incrementRunningFileFixer() {}

        @Override
        public void reset(DataFixTypes dataFixType) {}
    }
}
