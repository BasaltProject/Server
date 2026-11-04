// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import net.minecraft.CrashReportCategory;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelHeightAccessor;
public interface ServerLevelData extends WritableLevelData {
    default String getLevelName() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#getLevelName");
    }
    default void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#fillCrashReportCategory");
    }
    default GameType getGameType() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#getGameType");
    }
    default boolean isInitialized() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#isInitialized");
    }
    default void setInitialized(boolean initialized) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#setInitialized");
    }
    default boolean isAllowCommands() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#isAllowCommands");
    }
    default void setAllowCommands(boolean allowCommands) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#setAllowCommands");
    }
    default void setGameType(GameType gameType) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#setGameType");
    }
    default void setGameTime(long time) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.ServerLevelData#setGameTime");
    }
}
