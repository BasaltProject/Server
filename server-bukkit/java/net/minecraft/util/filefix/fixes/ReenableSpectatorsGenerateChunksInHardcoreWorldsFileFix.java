// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.Optional;
import net.minecraft.nbt.Tag;
import net.minecraft.util.datafix.fixes.References;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.access.FileAccess;
import net.minecraft.util.filefix.access.FileRelation;
import net.minecraft.util.filefix.access.FileResourceTypes;
import net.minecraft.util.filefix.access.LevelDat;
import net.minecraft.util.filefix.access.SavedDataNbt;
import net.minecraft.util.worldupdate.UpgradeProgress;

public class ReenableSpectatorsGenerateChunksInHardcoreWorldsFileFix extends FileFix {
    public ReenableSpectatorsGenerateChunksInHardcoreWorldsFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        addFileContentFix(files -> {
            FileAccess<LevelDat> levelDat = files.getFileAccess(FileResourceTypes.LEVEL_DAT,
                FileRelation.ORIGIN.forFile("level.dat"));
            FileAccess<SavedDataNbt> gameRules = files.getFileAccess(
                FileResourceTypes.savedData(References.SAVED_DATA_GAME_RULES),
                FileRelation.DATA.forFile("minecraft/game_rules.dat"));
            return progress -> {
                progress.setType(UpgradeProgress.Type.FILES);
                Optional<Dynamic<Tag>> level = levelDat.getOnlyFile().read();
                if (level.isEmpty()) {
                    return;
                }
                Dynamic<?> data = level.get();
                boolean everSingleplayer = data.get("singleplayer_uuid").get().isSuccess();
                boolean hardcore = data.get("difficulty_settings").orElseEmptyMap().get("hardcore").asBoolean(false);
                if (everSingleplayer && hardcore) {
                    SavedDataNbt file = gameRules.getOnlyFile();
                    file.read().ifPresent(rules -> file.write(
                        rules.update("minecraft:spectators_generate_chunks", old -> old.createBoolean(true))));
                }
            };
        });
    }
}
