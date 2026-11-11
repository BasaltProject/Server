// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
import com.mojang.serialization.Lifecycle;
import java.util.Set;
import java.util.UUID;
import net.minecraft.CrashReportCategory;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.Difficulty;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;
public interface WorldData {
    int ANVIL_VERSION_ID = 0;
    int MCREGION_VERSION_ID = 0;
    WorldDataConfiguration getDataConfiguration();
    void setDataConfiguration(WorldDataConfiguration dataConfiguration);
    boolean wasModded();
    Set<String> getKnownServerBrands();
    Set<String> getRemovedFeatureFlags();
    void setModdedInfo(String serverBrand, boolean isModded);
    default void fillCrashReportCategory(CrashReportCategory category) {
        category.setDetail("Known server brands", () -> String.join(", ", getKnownServerBrands()));
        category.setDetail("Removed feature flags", () -> String.join(", ", getRemovedFeatureFlags()));
        category.setDetail("Level was modded", () -> Boolean.toString(wasModded()));
        category.setDetail("Level storage version", () -> String.format(java.util.Locale.ROOT, "0x%05X - %s", getVersion(), getStorageVersionName(getVersion())));
    }
    default String getStorageVersionName(int version) {
        return version == 19132 ? "McRegion" : version == 19133 ? "Anvil" : "Unknown?";
    }
    ServerLevelData overworldData();
    LevelSettings getLevelSettings();
    CompoundTag createTag(UUID singlePlayerUUID);
    boolean isHardcore();
    int getVersion();
    String getLevelName();
    GameType getGameType();
    void setGameType(GameType gameType);
    boolean isAllowCommands();
    void setAllowCommands(boolean allowCommands);
    Difficulty getDifficulty();
    void setDifficulty(Difficulty difficulty);
    boolean isDifficultyLocked();
    void setDifficultyLocked(boolean difficultyLocked);
    UUID getSinglePlayerUUID();
    boolean isFlatWorld();
    boolean isDebugWorld();
    Lifecycle worldGenSettingsLifecycle();
    default FeatureFlagSet enabledFeatures() {
        return getDataConfiguration().enabledFeatures();
    }
}
