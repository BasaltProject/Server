// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.serialization.Lifecycle;
import java.util.Set;
import java.util.UUID;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;

/** The level data of the main world read through the Bukkit world; the pieces with no native source are null. */
public final class BasaltWorldData implements WorldData {
    private static org.bukkit.World world() {
        return org.bukkit.Bukkit.getWorlds().get(0);
    }

    public WorldDataConfiguration getDataConfiguration() {
        return null;
    }

    public void setDataConfiguration(WorldDataConfiguration dataConfiguration) {
    }

    public boolean wasModded() {
        return false;
    }

    public Set<String> getKnownServerBrands() {
        return Set.of("vanilla");
    }

    public Set<String> getRemovedFeatureFlags() {
        return Set.of();
    }

    public void setModdedInfo(String serverBrand, boolean isModded) {
    }

    public ServerLevelData overworldData() {
        return null;
    }

    public LevelSettings getLevelSettings() {
        return null;
    }

    public CompoundTag createTag(UUID singlePlayerUUID) {
        return new CompoundTag();
    }

    public boolean isHardcore() {
        return org.bukkit.Bukkit.isHardcore();
    }

    public int getVersion() {
        return ANVIL_VERSION_ID;
    }

    public String getLevelName() {
        return world().getName();
    }

    public GameType getGameType() {
        return GameType.byId(org.bukkit.Bukkit.getDefaultGameMode().getValue());
    }

    public void setGameType(GameType gameType) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("gamemode", gameType.getName());
    }

    public boolean isAllowCommands() {
        return true;
    }

    public void setAllowCommands(boolean allowCommands) {
    }

    public Difficulty getDifficulty() {
        return Difficulty.valueOf(world().getDifficulty().name());
    }

    public void setDifficulty(Difficulty difficulty) {
        world().setDifficulty(org.bukkit.Difficulty.valueOf(difficulty.name()));
    }

    public boolean isDifficultyLocked() {
        return false;
    }

    public void setDifficultyLocked(boolean difficultyLocked) {
    }

    public UUID getSinglePlayerUUID() {
        return null;
    }

    public boolean isFlatWorld() {
        return world().getWorldType() == org.bukkit.WorldType.FLAT;
    }

    public boolean isDebugWorld() {
        return false;
    }

    public Lifecycle worldGenSettingsLifecycle() {
        return Lifecycle.stable();
    }
}
