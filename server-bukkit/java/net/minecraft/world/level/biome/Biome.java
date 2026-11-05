// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.Identifier;
import net.minecraft.util.Mth;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.EnvironmentAttributeMap;
import net.minecraft.world.attribute.modifier.AttributeModifier;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.LiquidBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.synth.Noise;

public class Biome {
    public static final Codec<Biome> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Biome.class, "DIRECT_CODEC");
    public static final Codec<Biome> NETWORK_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Biome.class, "NETWORK_CODEC");
    public static final Codec<Holder<Biome>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Biome.class, "CODEC");
    public static final Codec<HolderSet<Biome>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(Biome.class, "LIST_CODEC");
    public static final Noise FROZEN_TEMPERATURE_NOISE = BiomeNoises.FROZEN_TEMPERATURE;
    public static final Noise BIOME_INFO_NOISE = BiomeNoises.INFO;
    private static final float kSnowLine = 0.15F;
    private final Identifier key;
    private volatile boolean loaded;
    private ClimateSettings climateSettings;
    private EnvironmentAttributeMap attributes;
    private BiomeSpecialEffects specialEffects;
    private BiomeGenerationSettings generationSettings;

    public Biome(Identifier key) {
        this.key = key;
    }

    private Biome(ClimateSettings climateSettings, EnvironmentAttributeMap attributes, BiomeSpecialEffects specialEffects, BiomeGenerationSettings generationSettings) {
        this.key = null;
        this.climateSettings = climateSettings;
        this.attributes = attributes;
        this.specialEffects = specialEffects;
        this.generationSettings = generationSettings;
        this.loaded = true;
    }

    private void load() {
        if (loaded) {
            return;
        }
        synchronized (this) {
            if (loaded) {
                return;
            }
            byte[] bytes = org.basaltmc.bukkit.LeftoverNatives.registryData("worldgen/biome", key.toString());
            Map<String, Object> facts = bytes == null ? Map.of() : org.basaltmc.bukkit.item.Nbt.read(bytes);
            TemperatureModifier modifier = "frozen".equals(facts.get("temperature_modifier")) ? TemperatureModifier.FROZEN : TemperatureModifier.NONE;
            climateSettings = new ClimateSettings(BiomeFacts.flag(facts.get("has_precipitation"), true),
                BiomeFacts.number(facts.get("temperature"), 0.8F), modifier, BiomeFacts.number(facts.get("downfall"), 0.4F));
            attributes = BiomeFacts.attributes(facts);
            specialEffects = BiomeFacts.effects(facts);
            generationSettings = BiomeGenerationSettings.EMPTY;
            loaded = true;
        }
    }

    public Identifier biomeKey() {
        return key;
    }

    @Override
    public String toString() {
        return key == null ? super.toString() : key.toString();
    }

    public boolean hasPrecipitation() {
        load();
        return climateSettings.hasPrecipitation();
    }

    public Precipitation getPrecipitationAt(BlockPos pos, int seaLevel) {
        if (!hasPrecipitation()) {
            return Precipitation.NONE;
        }
        return coldEnoughToSnow(pos, seaLevel) ? Precipitation.SNOW : Precipitation.RAIN;
    }

    private float getTemperature(BlockPos pos, int seaLevel) {
        load();
        float adjusted = climateSettings.temperatureModifier().modifyTemperature(pos, getBaseTemperature());
        int snowLevel = seaLevel + 17;
        if (pos.getY() <= snowLevel) {
            return adjusted;
        }
        float wobble = BiomeNoises.TEMPERATURE.get(pos.getX() / 8.0F, pos.getZ() / 8.0F) * 8.0F;
        return adjusted - (wobble + pos.getY() - snowLevel) * 0.05F / 40.0F;
    }

    public boolean shouldFreeze(LevelReader level, BlockPos pos) {
        return shouldFreeze(level, pos, true);
    }

    public boolean shouldFreeze(LevelReader level, BlockPos pos, boolean checkNeighbors) {
        if (warmEnoughToRain(pos, level.getSeaLevel())) {
            return false;
        }
        if (!level.isInsideBuildHeight(pos.getY()) || level.getBrightness(LightLayer.BLOCK, pos) >= 10) {
            return false;
        }
        BlockState state = level.getBlockState(pos);
        if (!level.getFluidState(pos).is(net.minecraft.world.level.material.Fluids.WATER) || !(state.getBlock() instanceof LiquidBlock)) {
            return false;
        }
        if (!checkNeighbors) {
            return true;
        }
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (!level.isWaterAt(pos.relative(side))) {
                return true;
            }
        }
        return false;
    }

    public boolean coldEnoughToSnow(BlockPos pos, int seaLevel) {
        return !warmEnoughToRain(pos, seaLevel);
    }

    public boolean warmEnoughToRain(BlockPos pos, int seaLevel) {
        return getTemperature(pos, seaLevel) >= kSnowLine;
    }

    public boolean shouldMeltFrozenOceanIcebergSlightly(BlockPos pos, int seaLevel) {
        return getTemperature(pos, seaLevel) > 0.1F;
    }

    public boolean shouldSnow(LevelReader level, BlockPos pos) {
        if (getPrecipitationAt(pos, level.getSeaLevel()) != Precipitation.SNOW) {
            return false;
        }
        if (!level.isInsideBuildHeight(pos.getY()) || level.getBrightness(LightLayer.BLOCK, pos) >= 10) {
            return false;
        }
        BlockState state = level.getBlockState(pos);
        return (state.isAir() || state.is(Blocks.SNOW)) && Blocks.SNOW.defaultBlockState().canSurvive(level, pos);
    }

    public BiomeGenerationSettings getGenerationSettings() {
        load();
        return generationSettings;
    }

    public int getGrassColor(double x, double z) {
        load();
        int base = specialEffects.grassColorOverride().orElseGet(() -> ClimateColors.grass(climateSettings.temperature(), climateSettings.downfall()));
        return specialEffects.grassColorModifier().modifyColor(x, z, base);
    }

    public int getFoliageColor() {
        load();
        return specialEffects.foliageColorOverride().orElseGet(() -> ClimateColors.foliage(climateSettings.temperature(), climateSettings.downfall()));
    }

    public int getDryFoliageColor() {
        load();
        return specialEffects.dryFoliageColorOverride().orElse(ClimateColors.kDryFoliage);
    }

    public float getBaseTemperature() {
        load();
        return climateSettings.temperature();
    }

    public EnvironmentAttributeMap getAttributes() {
        load();
        return attributes;
    }

    public BiomeSpecialEffects getSpecialEffects() {
        load();
        return specialEffects;
    }

    public int getWaterColor() {
        return getSpecialEffects().waterColor();
    }

    public enum Precipitation implements StringRepresentable {
        NONE, RAIN, SNOW;
        public static final Codec<Precipitation> CODEC = StringRepresentable.fromEnum(Precipitation::values);

        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }

    public enum TemperatureModifier implements StringRepresentable {
        NONE, FROZEN;
        public static final Codec<TemperatureModifier> CODEC = StringRepresentable.fromEnum(TemperatureModifier::values);

        public float modifyTemperature(BlockPos pos, float baseTemperature) {
            if (this == NONE) {
                return baseTemperature;
            }
            double patches = FROZEN_TEMPERATURE_NOISE.get(pos.getX() * 0.05, pos.getZ() * 0.05) * 7.0F
                + BIOME_INFO_NOISE.get(pos.getX() * 0.2, pos.getZ() * 0.2);
            if (patches < 0.3 && BIOME_INFO_NOISE.get(pos.getX() * 0.09, pos.getZ() * 0.09) < 0.8) {
                return 0.2F;
            }
            return baseTemperature;
        }

        public String getName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }

    public static class BiomeBuilder {
        private boolean hasPrecipitation = true;
        private Float temperature;
        private Float downfall;
        private TemperatureModifier temperatureModifier = TemperatureModifier.NONE;
        private final EnvironmentAttributeMap.Builder attributes = EnvironmentAttributeMap.builder();
        private BiomeSpecialEffects specialEffects;
        private BiomeGenerationSettings generationSettings;

        public BiomeBuilder() {}

        public BiomeBuilder hasPrecipitation(boolean hasPrecipitation) {
            this.hasPrecipitation = hasPrecipitation;
            return this;
        }

        public BiomeBuilder temperature(float temperature) {
            this.temperature = temperature;
            return this;
        }

        public BiomeBuilder downfall(float downfall) {
            this.downfall = downfall;
            return this;
        }

        public BiomeBuilder putAttributes(EnvironmentAttributeMap attributes) {
            this.attributes.putAll(attributes);
            return this;
        }

        public BiomeBuilder putAttributes(EnvironmentAttributeMap.Builder attributes) {
            return putAttributes(attributes.build());
        }

        public <Value> BiomeBuilder setAttribute(EnvironmentAttribute<Value> attribute, Value value) {
            attributes.set(attribute, value);
            return this;
        }

        public <Value, Parameter> BiomeBuilder modifyAttribute(EnvironmentAttribute<Value> attribute, AttributeModifier<Value, Parameter> modifier, Parameter value) {
            attributes.modify(attribute, modifier, value);
            return this;
        }

        public BiomeBuilder specialEffects(BiomeSpecialEffects specialEffects) {
            this.specialEffects = specialEffects;
            return this;
        }

        public BiomeBuilder mobSpawnSettings(MobSpawnSettings mobSpawnSettings) {
            attributes.set(net.minecraft.world.attribute.EnvironmentAttributes.NATURAL_MOB_SPAWNS, mobSpawnSettings);
            return this;
        }

        public BiomeBuilder generationSettings(BiomeGenerationSettings generationSettings) {
            this.generationSettings = generationSettings;
            return this;
        }

        public BiomeBuilder temperatureAdjustment(TemperatureModifier temperatureModifier) {
            this.temperatureModifier = temperatureModifier;
            return this;
        }

        public Biome build() {
            if (temperature == null || downfall == null || specialEffects == null || generationSettings == null) {
                throw new IllegalStateException("You are missing parameters to build a proper biome\n" + this);
            }
            return new Biome(new ClimateSettings(hasPrecipitation, temperature, temperatureModifier, downfall), attributes.build(), specialEffects, generationSettings);
        }

        @Override
        public String toString() {
            return "BiomeBuilder{temperature=" + temperature + ", downfall=" + downfall + ", precipitation=" + hasPrecipitation
                + ", effects=" + specialEffects + ", generation=" + generationSettings + "}";
        }
    }

    record ClimateSettings(boolean hasPrecipitation, float temperature, TemperatureModifier temperatureModifier, float downfall) {
        public static final MapCodec<ClimateSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(ClimateSettings.class, "CODEC");
    }
}
