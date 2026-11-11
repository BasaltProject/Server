// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
import net.minecraft.CrashReportCategory;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelHeightAccessor;
public class DerivedLevelData implements ServerLevelData {
    private final WorldData worldData;
    private final ServerLevelData wrapped;
    public DerivedLevelData(WorldData worldData, ServerLevelData wrapped) {
        this.worldData = worldData;
        this.wrapped = wrapped;
    }
    public LevelData.RespawnData getRespawnData() {
        return wrapped.getRespawnData();
    }
    public long getGameTime() {
        return wrapped.getGameTime();
    }
    public String getLevelName() {
        return worldData.getLevelName();
    }
    public GameType getGameType() {
        return worldData.getGameType();
    }
    public void setGameTime(long time) {
    }
    public void setSpawn(LevelData.RespawnData respawnData) {
        wrapped.setSpawn(respawnData);
    }
    public void setGameType(GameType gameType) {
    }
    public boolean isHardcore() {
        return worldData.isHardcore();
    }
    public boolean isAllowCommands() {
        return worldData.isAllowCommands();
    }
    public void setAllowCommands(boolean allowCommands) {
    }
    public boolean isInitialized() {
        return wrapped.isInitialized();
    }
    public void setInitialized(boolean initialized) {
    }
    public Difficulty getDifficulty() {
        return worldData.getDifficulty();
    }
    public boolean isDifficultyLocked() {
        return worldData.isDifficultyLocked();
    }
    public void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        category.setDetail("Derived", true);
        wrapped.fillCrashReportCategory(category, levelHeightAccessor);
    }
}
