// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import net.minecraft.CrashReportCategory;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelHeightAccessor;
public interface ServerLevelData extends WritableLevelData {
    default String getLevelName() {
        return "";
    }
    default void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        return;
    }
    default GameType getGameType() {
        return GameType.SURVIVAL;
    }
    default boolean isInitialized() {
        return true;
    }
    default void setInitialized(boolean initialized) {
        return;
    }
    default boolean isAllowCommands() {
        return true;
    }
    default void setAllowCommands(boolean allowCommands) {
        return;
    }
    default void setGameType(GameType gameType) {
        return;
    }
    default void setGameTime(long time) {
        return;
    }
}
