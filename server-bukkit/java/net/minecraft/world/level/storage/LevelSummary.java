// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import java.nio.file.Path;
import net.minecraft.SharedConstants;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelSettings;

public class LevelSummary implements Comparable<LevelSummary> {
    public static final Component PLAY_WORLD = Component.translatable("selectWorld.select");
    public static final Component UPGRADE_AND_PLAY_WORLD = Component.translatable("selectWorld.upgrade_and_play");
    private final LevelSettings settings;
    private final LevelVersion levelVersion;
    private final String levelId;
    private final boolean requiresManualConversion;
    private final boolean requiresFileFixing;
    private final boolean locked;
    private final boolean experimental;
    private final Path icon;

    public LevelSummary(LevelSettings settings, LevelVersion levelVersion, String levelId, boolean requiresManualConversion, boolean requiresFileFixing, boolean locked, boolean experimental, Path icon) {
        this.settings = settings;
        this.levelVersion = levelVersion;
        this.levelId = levelId;
        this.requiresManualConversion = requiresManualConversion;
        this.requiresFileFixing = requiresFileFixing;
        this.locked = locked;
        this.experimental = experimental;
        this.icon = icon;
    }

    public String getLevelId() {
        return levelId;
    }

    public String getLevelName() {
        return settings.levelName().isEmpty() ? levelId : settings.levelName();
    }

    public Path getIcon() {
        return icon;
    }

    public boolean requiresManualConversion() {
        return requiresManualConversion;
    }

    public boolean requiresFileFixing() {
        return requiresFileFixing;
    }

    public boolean isExperimental() {
        return experimental;
    }

    public long getLastPlayed() {
        return levelVersion.lastPlayed();
    }

    @Override
    public int compareTo(LevelSummary rhs) {
        if (getLastPlayed() != rhs.getLastPlayed()) {
            return getLastPlayed() < rhs.getLastPlayed() ? 1 : -1;
        }
        return levelId.compareTo(rhs.levelId);
    }

    public LevelSettings getSettings() {
        return settings;
    }

    public GameType getGameMode() {
        return settings.gameType();
    }

    public boolean isHardcore() {
        return settings.difficultySettings().hardcore();
    }

    public boolean hasCommands() {
        return settings.allowCommands();
    }

    public MutableComponent getWorldVersionName() {
        String name = levelVersion.minecraftVersionName();
        return name.isEmpty() ? Component.translatable("selectWorld.versionUnknown") : Component.literal(name);
    }

    public LevelVersion levelVersion() {
        return levelVersion;
    }

    public boolean shouldBackup() {
        return backupStatus().shouldBackup();
    }

    public boolean isDowngrade() {
        return backupStatus() == BackupStatus.DOWNGRADE;
    }

    public BackupStatus backupStatus() {
        int stored = levelVersion.minecraftVersion().version();
        int current = SharedConstants.getCurrentVersion().dataVersion().version();
        if (stored > current) {
            return BackupStatus.DOWNGRADE;
        }
        return stored < current && levelVersion.snapshot() ? BackupStatus.UPGRADE_TO_SNAPSHOT : BackupStatus.NONE;
    }

    public boolean isLocked() {
        return locked;
    }

    public boolean isDisabled() {
        return requiresManualConversion || !isCompatible();
    }

    public boolean isCompatible() {
        return levelVersion.minecraftVersion().isCompatible(SharedConstants.getCurrentVersion().dataVersion());
    }

    public Component getInfo() {
        if (isLocked()) {
            return Component.translatable("selectWorld.locked");
        }
        return requiresManualConversion ? Component.translatable("selectWorld.conversion") : Component.translatable(isHardcore() ? "gameMode.hardcore" : "gameMode." + getGameMode().getName());
    }

    public Component primaryActionMessage() {
        return shouldBackup() ? UPGRADE_AND_PLAY_WORLD : PLAY_WORLD;
    }

    public boolean primaryActionActive() {
        return !isDisabled();
    }

    public boolean canUpload() {
        return !requiresManualConversion && !requiresFileFixing;
    }

    public boolean canEdit() {
        return !requiresManualConversion && !requiresFileFixing;
    }

    public boolean canRecreate() {
        return canEdit();
    }

    public boolean canDelete() {
        return true;
    }

    public enum BackupStatus {
        NONE, DOWNGRADE, UPGRADE_TO_SNAPSHOT, FILE_FIXING_REQUIRED;

        public boolean shouldBackup() {
            return this != NONE;
        }

        public boolean isSevere() {
            return this == DOWNGRADE || this == FILE_FIXING_REQUIRED;
        }

        public String getTranslationKey() {
            return "selectWorld.backupStatus." + name().toLowerCase(java.util.Locale.ROOT);
        }
    }

    public static class SymlinkLevelSummary extends LevelSummary {
        public SymlinkLevelSummary(String levelId, Path icon) {
            super(null, null, levelId, false, false, false, false, icon);
        }

        @Override
        public String getLevelName() {
            return getLevelId();
        }

        @Override
        public Component getInfo() {
            return Component.translatable("symlink_warning.title");
        }

        @Override
        public long getLastPlayed() {
            return -1L;
        }

        @Override
        public boolean isDisabled() {
            return false;
        }

        @Override
        public Component primaryActionMessage() {
            return Component.translatable("symlink_warning.more_info");
        }

        @Override
        public boolean primaryActionActive() {
            return true;
        }

        @Override
        public boolean canUpload() {
            return false;
        }

        @Override
        public boolean canEdit() {
            return false;
        }

        @Override
        public boolean canRecreate() {
            return false;
        }
    }

    public static class CorruptedLevelSummary extends LevelSummary {
        private final long lastPlayed;

        public CorruptedLevelSummary(String levelId, Path icon, long lastPlayed) {
            super(null, null, levelId, false, false, false, false, icon);
            this.lastPlayed = lastPlayed;
        }

        @Override
        public String getLevelName() {
            return getLevelId();
        }

        @Override
        public Component getInfo() {
            return Component.translatable("recover_world.state_entry");
        }

        @Override
        public long getLastPlayed() {
            return lastPlayed;
        }

        @Override
        public boolean isDisabled() {
            return false;
        }

        @Override
        public Component primaryActionMessage() {
            return Component.translatable("recover_world.action");
        }

        @Override
        public boolean primaryActionActive() {
            return true;
        }

        @Override
        public boolean canUpload() {
            return false;
        }

        @Override
        public boolean canEdit() {
            return false;
        }

        @Override
        public boolean canRecreate() {
            return false;
        }
    }
}
