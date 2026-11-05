// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import net.minecraft.world.Difficulty;

public record LevelSettings(String levelName, GameType gameType, DifficultySettings difficultySettings, boolean allowCommands, WorldDataConfiguration dataConfiguration) {
    public static LevelSettings parse(Dynamic<?> input, WorldDataConfiguration loadConfig) {
        GameType mode = GameType.byId(input.get("GameType").asInt(0));
        boolean hardcore = input.get("hardcore").asBoolean(false);
        Difficulty difficulty = input.get("Difficulty").asNumber().result().map(number -> Difficulty.byId(number.intValue())).orElse(Difficulty.NORMAL);
        boolean locked = input.get("DifficultyLocked").asBoolean(false);
        return new LevelSettings(input.get("LevelName").asString(""), mode, new DifficultySettings(difficulty, hardcore, locked),
            input.get("allowCommands").asBoolean(mode == GameType.CREATIVE), loadConfig);
    }

    public LevelSettings withGameType(GameType gameType) {
        return new LevelSettings(levelName, gameType, difficultySettings, allowCommands, dataConfiguration);
    }

    public LevelSettings withAllowCommands(boolean allowCommands) {
        return new LevelSettings(levelName, gameType, difficultySettings, allowCommands, dataConfiguration);
    }

    public LevelSettings withDifficulty(Difficulty difficulty) {
        return new LevelSettings(levelName, gameType, new DifficultySettings(difficulty, difficultySettings.hardcore(), difficultySettings.locked()), allowCommands, dataConfiguration);
    }

    public LevelSettings withDifficultyLock(boolean locked) {
        return new LevelSettings(levelName, gameType, new DifficultySettings(difficultySettings.difficulty(), difficultySettings.hardcore(), locked), allowCommands, dataConfiguration);
    }

    public LevelSettings withDataConfiguration(WorldDataConfiguration dataConfiguration) {
        return new LevelSettings(levelName, gameType, difficultySettings, allowCommands, dataConfiguration);
    }

    public LevelSettings copy() {
        return new LevelSettings(levelName, gameType, difficultySettings, allowCommands, dataConfiguration);
    }

    public record DifficultySettings(Difficulty difficulty, boolean hardcore, boolean locked) {
        public static final DifficultySettings DEFAULT = new DifficultySettings(Difficulty.NORMAL, false, false);
        public static final Codec<DifficultySettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(DifficultySettings.class, "CODEC");
    }
}
