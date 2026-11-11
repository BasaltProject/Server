// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.world.flag.FeatureFlags;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.InclusiveRange;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.entity.EquipmentTable;
public record SpawnData(CompoundTag entityToSpawn, Optional<CustomSpawnRules> customSpawnRules, Optional<EquipmentTable> equipment) {
    public SpawnData {
        Optional<Identifier> id = entityToSpawn.read("id", Identifier.CODEC);
        if (id.isPresent()) {
            entityToSpawn.store("id", Identifier.CODEC, id.get());
        } else {
            entityToSpawn.remove("id");
        }
    }
    public static final String ENTITY_TAG = "entity";
    public static final Codec<SpawnData> CODEC = RecordCodecBuilder.create(i -> i.group(
    CompoundTag.CODEC.fieldOf("entity").forGetter(SpawnData::entityToSpawn),
    CustomSpawnRules.CODEC.optionalFieldOf("custom_spawn_rules").forGetter(SpawnData::customSpawnRules),
    EquipmentTable.CODEC.optionalFieldOf("equipment").forGetter(SpawnData::equipment)).apply(i, SpawnData::new));
    public static final Codec<WeightedList<SpawnData>> LIST_CODEC = WeightedList.codec(CODEC);
    public SpawnData() {
        this(new CompoundTag(), Optional.empty(), Optional.empty());
    }
    public CompoundTag getEntityToSpawn() {
        return entityToSpawn;
    }
    public Optional<CustomSpawnRules> getCustomSpawnRules() {
        return customSpawnRules;
    }
    public Optional<EquipmentTable> getEquipment() {
        return equipment;
    }
    public static record CustomSpawnRules(InclusiveRange<Integer> blockLightLimit, InclusiveRange<Integer> skyLightLimit) {
        private static final InclusiveRange<Integer> kLightRange = new InclusiveRange<>(0, 15);
        private static DataResult<InclusiveRange<Integer>> checkLightBoundaries(InclusiveRange<Integer> range) {
            return kLightRange.contains(range) ? DataResult.success(range) : DataResult.error(() -> "Light values must be withing range " + kLightRange);
        }
        private static MapCodec<InclusiveRange<Integer>> lightLimit(String name) {
            return InclusiveRange.INT.lenientOptionalFieldOf(name, kLightRange).validate(CustomSpawnRules::checkLightBoundaries);
        }
        public static final Codec<CustomSpawnRules> CODEC = RecordCodecBuilder.create(i -> i.group(
    lightLimit("block_light_limit").forGetter(CustomSpawnRules::blockLightLimit),
    lightLimit("sky_light_limit").forGetter(CustomSpawnRules::skyLightLimit)).apply(i, CustomSpawnRules::new));
        public boolean isValidPosition(BlockPos blockSpawnPos, ServerLevel level) {
        return blockLightLimit.isValueInRange(level.getBrightness(LightLayer.BLOCK, blockSpawnPos))
            && skyLightLimit.isValueInRange(level.getEffectiveSkyBrightness(blockSpawnPos));
        }
    }
}
