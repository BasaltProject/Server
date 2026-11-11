// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.DSL;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.Arrays;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.core.UUIDUtil;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.nbt.Tag;
import net.minecraft.util.Util;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.util.datafix.DataFixers;
import net.minecraft.util.datafix.fixes.References;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.access.CompressedNbt;
import net.minecraft.util.filefix.access.FileAccess;
import net.minecraft.util.filefix.access.FileRelation;
import net.minecraft.util.filefix.access.FileResourceTypes;
import net.minecraft.util.filefix.access.LevelDat;
import net.minecraft.util.filefix.access.PlayerData;
import net.minecraft.util.filefix.access.SavedDataNbt;
import net.minecraft.util.worldupdate.UpgradeProgress;

public class LevelDatToSavedDataFileFix extends FileFix {
    private static final UUID FALLBACK_SINGLE_PLAYER_UUID = Util.NIL_UUID;

    public LevelDatToSavedDataFileFix(Schema schema) {
        super(schema);
    }

    private static FileAccess<SavedDataNbt> saved(net.minecraft.util.filefix.access.FileAccessProvider files,
            DSL.TypeReference type, FileRelation relation) {
        return files.getFileAccess(FileResourceTypes.savedData(type), relation);
    }

    @Override
    public void makeFixer() {
        addFileContentFix(files -> {
            FileAccess<LevelDat> levelDat = files.getFileAccess(FileResourceTypes.LEVEL_DAT,
                FileRelation.ORIGIN.forFile("level.dat"));
            FileAccess<SavedDataNbt> dragonFight = saved(files, References.SAVED_DATA_ENDER_DRAGON_FIGHT,
                FileRelation.forDataFileInDimension("the_end", "minecraft/ender_dragon_fight.dat"));
            FileAccess<PlayerData> fallbackPlayer = files.getFileAccess(FileResourceTypes.PLAYER_DATA,
                FileRelation.PLAYER_DATA.forFile(FALLBACK_SINGLE_PLAYER_UUID + ".dat"));
            FileAccess<SavedDataNbt> wanderingTrader = saved(files, References.SAVED_DATA_WANDERING_TRADER,
                FileRelation.DATA.forFile("minecraft/wandering_trader.dat"));
            FileAccess<SavedDataNbt> bossEvents = saved(files, References.SAVED_DATA_CUSTOM_BOSS_EVENTS,
                FileRelation.DATA.forFile("minecraft/custom_boss_events.dat"));
            FileAccess<SavedDataNbt> weather = saved(files, References.SAVED_DATA_WEATHER,
                FileRelation.DATA.forFile("minecraft/weather.dat"));
            FileAccess<SavedDataNbt> scheduledEvents = saved(files, References.SAVED_DATA_SCHEDULED_EVENTS,
                FileRelation.DATA.forFile("minecraft/scheduled_events.dat"));
            FileAccess<SavedDataNbt> borderOverworld = saved(files, References.SAVED_DATA_WORLD_BORDER,
                FileRelation.forDataFileInDimension("overworld", "minecraft/world_border.dat"));
            FileAccess<SavedDataNbt> borderNether = saved(files, References.SAVED_DATA_WORLD_BORDER,
                FileRelation.forDataFileInDimension("the_nether", "minecraft/world_border.dat"));
            FileAccess<SavedDataNbt> borderEnd = saved(files, References.SAVED_DATA_WORLD_BORDER,
                FileRelation.forDataFileInDimension("the_end", "minecraft/world_border.dat"));
            FileAccess<SavedDataNbt> gameRules = saved(files, References.SAVED_DATA_GAME_RULES,
                FileRelation.DATA.forFile("minecraft/game_rules.dat"));
            FileAccess<SavedDataNbt> worldGenSettings = saved(files, References.SAVED_DATA_WORLD_GEN_SETTINGS,
                FileRelation.DATA.forFile("minecraft/world_gen_settings.dat"));
            FileAccess<SavedDataNbt> worldClocks = saved(files, References.SAVED_DATA_WORLD_CLOCKS,
                FileRelation.DATA.forFile("minecraft/world_clocks.dat"));
            return progress -> {
                progress.setType(UpgradeProgress.Type.FILES);
                LevelDat levelFile = levelDat.getOnlyFile();
                Optional<Dynamic<Tag>> read = levelFile.read();
                if (read.isEmpty()) {
                    return;
                }
                Dynamic<?> content = read.get();
                content = extractToFile(dragonFight, content, "dragon_fight");
                content = extractPlayer(fallbackPlayer, content);
                content = extractToFile(wanderingTrader, content, "wandering_trader_migration_data");
                content = extractToFile(bossEvents, content, "CustomBossEvents");
                content = extractToFile(weather, content, "weather_data");
                content = extractToFile(scheduledEvents, content, "scheduled_events");
                content = extractWorldBorders(borderOverworld, borderNether, borderEnd, content);
                content = extractToFile(gameRules, content, "game_rules");
                content = extractWorldGenSettings(worldGenSettings, content);
                content = extractToFile(worldClocks, content, "world_clocks");
                levelFile.write(content);
            };
        });
    }

    private static Dynamic<?> extractToFile(FileAccess<? extends CompressedNbt> target, Dynamic<?> content, String key) {
        Optional<? extends Dynamic<?>> tag = content.get(key).result();
        if (tag.isEmpty()) {
            return content;
        }
        target.getOnlyFile().write(tag.get());
        return content.remove(key);
    }

    private Dynamic<?> extractPlayer(FileAccess<PlayerData> fallback, Dynamic<?> content) {
        Optional<? extends Dynamic<?>> player = content.get("Player").result();
        if (player.isEmpty()) {
            return content;
        }
        Dynamic<?> tag = player.get();
        Dynamic<?> fixed = DataFixTypes.PLAYER.update(DataFixers.getDataFixer(), tag, NbtUtils.getDataVersion(tag),
            getVersion());
        Optional<? extends Dynamic<?>> uuid = fixed.get("UUID").result();
        Dynamic<?> used;
        if (uuid.isPresent()) {
            used = uuid.get();
        } else {
            fallback.getOnlyFile().write(fixed);
            used = content.createIntList(Arrays.stream(UUIDUtil.uuidToIntArray(FALLBACK_SINGLE_PLAYER_UUID)));
        }
        return content.remove("Player").set("singleplayer_uuid", used);
    }

    private static Dynamic<?> extractWorldBorders(FileAccess<? extends CompressedNbt> overworld,
            FileAccess<? extends CompressedNbt> nether, FileAccess<? extends CompressedNbt> end, Dynamic<?> content) {
        extractWorldBorder(overworld, content, 1.0);
        extractWorldBorder(nether, content, 8.0);
        extractWorldBorder(end, content, 1.0);
        return content.remove("world_border");
    }

    private static void extractWorldBorder(FileAccess<? extends CompressedNbt> target, Dynamic<?> content,
            double divider) {
        Optional<? extends Dynamic<?>> border = content.get("world_border").result();
        if (border.isPresent()) {
            target.getOnlyFile().write(border.get()
                .update("center_x", x -> x.createDouble(x.asDouble(0.0) / divider))
                .update("center_z", z -> z.createDouble(z.asDouble(0.0) / divider)));
        }
    }

    private Dynamic<?> extractWorldGenSettings(FileAccess<? extends CompressedNbt> target, Dynamic<?> content) {
        Optional<? extends Dynamic<?>> settings = content.get("world_gen_settings").result();
        if (settings.isEmpty()) {
            return content;
        }
        Dynamic<?> fixed = DataFixTypes.WORLD_GEN_SETTINGS.update(DataFixers.getDataFixer(), settings.get(),
            NbtUtils.getDataVersion(content), getVersion());
        target.getOnlyFile().write(fixed);
        return content.remove("world_gen_settings");
    }
}
