// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix;

import com.mojang.datafixers.DSL;
import com.mojang.datafixers.DataFixer;
import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import java.util.Set;
import net.minecraft.SharedConstants;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;

public enum DataFixTypes {
    LEVEL, LEVEL_SUMMARY, PLAYER, CHUNK, HOTBAR, OPTIONS, STRUCTURE, STATS, SAVED_DATA_COMMAND_STORAGE, SAVED_DATA_CUSTOM_BOSS_EVENTS, SAVED_DATA_ENDER_DRAGON_FIGHT, SAVED_DATA_GAME_RULES, SAVED_DATA_FORCED_CHUNKS, SAVED_DATA_MAP_DATA, SAVED_DATA_MAP_INDEX, SAVED_DATA_RAIDS, SAVED_DATA_RANDOM_SEQUENCES, SAVED_DATA_SCHEDULED_EVENTS, SAVED_DATA_SCOREBOARD, SAVED_DATA_STOPWATCHES, SAVED_DATA_STRUCTURE_FEATURE_INDICES, SAVED_DATA_WANDERING_TRADER, SAVED_DATA_WEATHER, SAVED_DATA_WORLD_BORDER, SAVED_DATA_WORLD_CLOCKS, SAVED_DATA_WORLD_GEN_SETTINGS, ADVANCEMENTS, POI_CHUNK, WORLD_GEN_SETTINGS, ENTITY_CHUNK, DEBUG_PROFILE;
    public static final Set<DSL.TypeReference> TYPES_FOR_LEVEL_LIST = Set.of();

    private static int current() {
        return SharedConstants.getCurrentVersion().dataVersion().version();
    }

    public <A> Codec<A> wrapCodec(Codec<A> codec, DataFixer dataFixer, int defaultVersion) {
        return codec;
    }

    /** The name the native fixer gives the data of this type. */
    public String nativeType() {
        return switch (this) {
            case LEVEL, LEVEL_SUMMARY -> "level";
            case PLAYER -> "player";
            case CHUNK -> "chunk";
            case HOTBAR -> "hotbar";
            case OPTIONS -> "options";
            case STRUCTURE -> "structure";
            case STATS -> "stats";
            case ADVANCEMENTS -> "advancements";
            case POI_CHUNK -> "poi_chunk";
            case WORLD_GEN_SETTINGS -> "world_gen_settings";
            case ENTITY_CHUNK -> "entity_chunk";
            case DEBUG_PROFILE -> "debug_profile";
            case SAVED_DATA_COMMAND_STORAGE -> "saved_data:command_storage";
            case SAVED_DATA_CUSTOM_BOSS_EVENTS -> "custom_boss_events";
            case SAVED_DATA_ENDER_DRAGON_FIGHT -> "saved_data:ender_dragon_fight";
            case SAVED_DATA_GAME_RULES -> "saved_data:game_rules";
            case SAVED_DATA_FORCED_CHUNKS -> "tickets";
            case SAVED_DATA_MAP_DATA -> "map";
            case SAVED_DATA_MAP_INDEX -> "map_index";
            case SAVED_DATA_RAIDS -> "raids";
            case SAVED_DATA_RANDOM_SEQUENCES -> "random_sequences";
            case SAVED_DATA_SCHEDULED_EVENTS -> "saved_data:scheduled_events";
            case SAVED_DATA_SCOREBOARD -> "scoreboard";
            case SAVED_DATA_STOPWATCHES -> "saved_data:stopwatches";
            case SAVED_DATA_STRUCTURE_FEATURE_INDICES -> "structure_feature_indices";
            case SAVED_DATA_WANDERING_TRADER -> "saved_data:wandering_trader";
            case SAVED_DATA_WEATHER -> "saved_data:weather";
            case SAVED_DATA_WORLD_BORDER -> "world_border";
            case SAVED_DATA_WORLD_CLOCKS -> "saved_data:world_clocks";
            case SAVED_DATA_WORLD_GEN_SETTINGS -> "saved_data:world_gen_settings";
        };
    }

    public <T> Dynamic<T> update(DataFixer fixerUpper, Dynamic<T> input, int fromVersion, int toVersion) {
        if (fromVersion >= toVersion) {
            return input;
        }
        String type = nativeType();
        Tag tag = input.convert(NbtOps.INSTANCE).getValue();
        byte[] fixed = org.basaltmc.bukkit.LeftoverNatives.datafix(type,
            org.basaltmc.bukkit.facade.NbtCodec.toAnonymous((CompoundTag) tag), fromVersion, toVersion);
        if (fixed == null) {
            String gap = org.basaltmc.bukkit.LeftoverNatives.datafixMissing(type, fromVersion, toVersion);
            throw new IllegalStateException("The native data fixer cannot bring " + name() + " data from " + fromVersion
                + (gap == null ? "" : ": no fix for " + gap));
        }
        return new Dynamic<Tag>(NbtOps.INSTANCE, org.basaltmc.bukkit.facade.NbtCodec.fromAnonymous(fixed)).convert(input.getOps());
    }

    public <T> Dynamic<T> updateToCurrentVersion(DataFixer fixerUpper, Dynamic<T> input, int dataVersion) {
        return update(fixerUpper, input, dataVersion, current());
    }

    public CompoundTag update(DataFixer fixer, CompoundTag tag, int fromVersion, int toVersion) {
        Dynamic<Tag> fixed = update(fixer, new Dynamic<>(NbtOps.INSTANCE, (Tag) tag), fromVersion, toVersion);
        return (CompoundTag) fixed.getValue();
    }

    public CompoundTag updateToCurrentVersion(DataFixer fixer, CompoundTag tag, int fromVersion) {
        return update(fixer, tag, fromVersion, current());
    }
}
