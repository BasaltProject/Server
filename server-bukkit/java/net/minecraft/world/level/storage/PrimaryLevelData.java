// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.Lifecycle;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import net.minecraft.CrashReportCategory;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;
public class PrimaryLevelData implements ServerLevelData, WorldData {
    private LevelSettings settings;
    private final SpecialWorldProperty specialWorldProperty;
    private final Lifecycle worldGenSettingsLifecycle;
    private LevelData.RespawnData respawnData;
    private long gameTime;
    private UUID singlePlayerUUID;
    private boolean wasModded;
    private int version;
    private boolean initialized;
    private final Set<String> knownServerBrands;
    private final Set<String> removedFeatureFlags;
    private final List<Integer> versionHistory;
    public static final String LEVEL_NAME = "LevelName";
    protected static final String OLD_PLAYER = "Player";
    protected static final String SINGLEPLAYER_UUID = "singleplayer_uuid";
    protected static final String OLD_WORLD_GEN_SETTINGS = "WorldGenSettings";
    private PrimaryLevelData(UUID singlePlayerUUID, boolean wasModded, LevelData.RespawnData respawnData, long gameTime, int version, boolean initialized, Set<String> knownServerBrands, Set<String> removedFeatureFlags, LevelSettings settings, SpecialWorldProperty specialWorldProperty, Lifecycle worldGenSettingsLifecycle, List<Integer> versionHistory) {
        this.singlePlayerUUID = singlePlayerUUID;
        this.wasModded = wasModded;
        this.respawnData = respawnData;
        this.gameTime = gameTime;
        this.version = version;
        this.initialized = initialized;
        this.knownServerBrands = knownServerBrands;
        this.removedFeatureFlags = removedFeatureFlags;
        this.settings = settings;
        this.specialWorldProperty = specialWorldProperty;
        this.worldGenSettingsLifecycle = worldGenSettingsLifecycle;
        this.versionHistory = versionHistory;
    }
    public PrimaryLevelData(LevelSettings levelSettings, SpecialWorldProperty specialWorldProperty, Lifecycle lifecycle) {
        this(null, false, LevelData.RespawnData.DEFAULT, 0L, 19133, false, new java.util.HashSet<>(), new java.util.HashSet<>(),
            levelSettings.copy(), specialWorldProperty, lifecycle, new java.util.ArrayList<>());
    }
    public static <T> PrimaryLevelData parse(Dynamic<T> input, LevelSettings settings, SpecialWorldProperty specialWorldProperty, Lifecycle worldGenSettingsLifecycle) {
        Set<String> brands = new java.util.HashSet<>();
        input.get("ServerBrands").asStream().forEach(entry -> entry.asString().result().ifPresent(brands::add));
        Set<String> removed = new java.util.HashSet<>();
        input.get("removed_features").asStream().forEach(entry -> entry.asString().result().ifPresent(removed::add));
        List<Integer> history = new java.util.ArrayList<>();
        input.get("version_history").asStream().forEach(entry -> history.add(entry.asInt(0)));
        UUID player = input.get("singleplayer_uuid").flatMap(net.minecraft.core.UUIDUtil.CODEC::parse).result().orElse(null);
        LevelData.RespawnData spawn = input.get("spawn").read(LevelData.RespawnData.CODEC).result().orElse(LevelData.RespawnData.DEFAULT);
        return new PrimaryLevelData(player, input.get("WasModded").asBoolean(false), spawn, input.get("Time").asLong(0L),
            LevelVersion.parse(input).levelDataVersion(), input.get("initialized").asBoolean(true), brands, removed, settings,
            specialWorldProperty, worldGenSettingsLifecycle, history);
    }
    public CompoundTag createTag(UUID singlePlayerUUID) {
        CompoundTag tag = new CompoundTag();
        UUID owner = singlePlayerUUID != null ? singlePlayerUUID : this.singlePlayerUUID;
        tag.putString("LevelName", settings.levelName());
        tag.putLong("Time", gameTime);
        tag.putBoolean("WasModded", wasModded);
        tag.putBoolean("initialized", initialized);
        tag.putBoolean("hardcore", settings.difficultySettings().hardcore());
        tag.putBoolean("allowCommands", settings.allowCommands());
        tag.putInt("GameType", settings.gameType().getId());
        tag.putInt("Difficulty", settings.difficultySettings().difficulty().getId());
        tag.putBoolean("DifficultyLocked", settings.difficultySettings().locked());
        if (owner != null) {
            tag.putIntArray("singleplayer_uuid", net.minecraft.core.UUIDUtil.uuidToIntArray(owner));
        }
        net.minecraft.nbt.ListTag brands = new net.minecraft.nbt.ListTag();
        knownServerBrands.forEach(brand -> brands.add(net.minecraft.nbt.StringTag.valueOf(brand)));
        tag.put("ServerBrands", brands);
        writeVersionTag(tag);
        writeVersionHistory(tag);
        writeLastPlayed(tag);
        return tag;
    }
    public static void writeLastPlayed(CompoundTag tag) {
        tag.putLong("LastPlayed", System.currentTimeMillis());
    }
    public static Dynamic<?> writeLastPlayed(Dynamic<?> tag) {
        return tag.set("LastPlayed", tag.createLong(System.currentTimeMillis()));
    }
    public static void writeVersionTag(CompoundTag tag) {
        var current = net.minecraft.SharedConstants.getCurrentVersion();
        CompoundTag stamp = new CompoundTag();
        stamp.putString("Name", current.name());
        stamp.putInt("Id", current.dataVersion().version());
        stamp.putBoolean("Snapshot", !current.stable());
        stamp.putString("Series", current.dataVersion().series());
        tag.put("Version", stamp);
    }
    public static Dynamic<?> writeVersionTag(Dynamic<?> tag) {
        var current = net.minecraft.SharedConstants.getCurrentVersion();
        Dynamic<?> stamp = tag.emptyMap().set("Name", tag.createString(current.name())).set("Id", tag.createInt(current.dataVersion().version()))
            .set("Snapshot", tag.createBoolean(!current.stable())).set("Series", tag.createString(current.dataVersion().series()));
        return tag.set("Version", stamp);
    }
    public void writeVersionHistory(CompoundTag tag) {
        net.minecraft.nbt.ListTag list = new net.minecraft.nbt.ListTag();
        versionHistory.forEach(each -> list.add(net.minecraft.nbt.IntTag.valueOf(each)));
        int current = net.minecraft.SharedConstants.getCurrentVersion().dataVersion().version();
        if (versionHistory.isEmpty() || versionHistory.getLast() != current) {
            list.add(net.minecraft.nbt.IntTag.valueOf(current));
        }
        tag.put("version_history", list);
    }
    public LevelData.RespawnData getRespawnData() {
        return respawnData;
    }
    public long getGameTime() {
        return gameTime;
    }
    public UUID getSinglePlayerUUID() {
        return singlePlayerUUID;
    }
    public void setGameTime(long time) {
        this.gameTime = time;
    }
    public void setSpawn(LevelData.RespawnData respawnData) {
        this.respawnData = respawnData;
    }
    public String getLevelName() {
        return settings.levelName();
    }
    public int getVersion() {
        return version;
    }
    public GameType getGameType() {
        return settings.gameType();
    }
    public void setGameType(GameType gameType) {
        this.settings = settings.withGameType(gameType);
    }
    public boolean isHardcore() {
        return settings.difficultySettings().hardcore();
    }
    public boolean isAllowCommands() {
        return settings.allowCommands();
    }
    public void setAllowCommands(boolean allowCommands) {
        this.settings = settings.withAllowCommands(allowCommands);
    }
    public boolean isInitialized() {
        return initialized;
    }
    public void setInitialized(boolean initialized) {
        this.initialized = initialized;
    }
    public Difficulty getDifficulty() {
        return settings.difficultySettings().difficulty();
    }
    public void setDifficulty(Difficulty difficulty) {
        this.settings = settings.withDifficulty(difficulty);
    }
    public boolean isDifficultyLocked() {
        return settings.difficultySettings().locked();
    }
    public void setDifficultyLocked(boolean difficultyLocked) {
        this.settings = settings.withDifficultyLock(difficultyLocked);
    }
    public void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        category.setDetail("Level name", getLevelName());
        category.setDetail("Level game mode", String.format("Game mode: %s (ID %d). Hardcore: %b. Commands: %b", getGameType().getName(), getGameType().getId(), isHardcore(), isAllowCommands()));
    }
    public boolean isFlatWorld() {
        return specialWorldProperty == SpecialWorldProperty.FLAT;
    }
    public boolean isDebugWorld() {
        return specialWorldProperty == SpecialWorldProperty.DEBUG;
    }
    public Lifecycle worldGenSettingsLifecycle() {
        return worldGenSettingsLifecycle;
    }
    public WorldDataConfiguration getDataConfiguration() {
        return settings.dataConfiguration();
    }
    public void setDataConfiguration(WorldDataConfiguration dataConfiguration) {
        this.settings = settings.withDataConfiguration(dataConfiguration);
    }
    public void setModdedInfo(String serverBrand, boolean isModded) {
        knownServerBrands.add(serverBrand);
        wasModded |= isModded;
    }
    public boolean wasModded() {
        return wasModded;
    }
    public Set<String> getKnownServerBrands() {
        return knownServerBrands;
    }
    public Set<String> getRemovedFeatureFlags() {
        return removedFeatureFlags;
    }
    public ServerLevelData overworldData() {
        return this;
    }
    public LevelSettings getLevelSettings() {
        return settings.copy();

    }
    public static enum SpecialWorldProperty {
        NONE, FLAT, DEBUG;
    }
}
