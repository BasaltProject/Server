// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Registry;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.util.TriState;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.level.MoonPhase;
import net.minecraft.world.level.biome.MobSpawnSettings;
import org.joml.Vector3fc;
import org.joml.Vector4fc;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.util.ARGB;
import net.minecraft.core.particles.ParticleTypes;

public interface EnvironmentAttributes {
    int kSync = 1;
    int kSpatial = 2;
    int kLocal = 4;
    int kFullBiomes = 8;

    EnvironmentAttribute<Vector3fc> FOG_COLOR = of("visual/fog_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(0), null, kSync | kSpatial);
    EnvironmentAttribute<Float> FOG_START_DISTANCE = of("visual/fog_start_distance", AttributeTypes.FLOAT, 0.0F, null, kSync | kSpatial);
    EnvironmentAttribute<Float> FOG_END_DISTANCE = of("visual/fog_end_distance", AttributeTypes.FLOAT, 1024.0F, AttributeRange.NON_NEGATIVE_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Float> SKY_FOG_END_DISTANCE = of("visual/sky_fog_end_distance", AttributeTypes.FLOAT, 512.0F, AttributeRange.NON_NEGATIVE_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Float> CLOUD_FOG_END_DISTANCE = of("visual/cloud_fog_end_distance", AttributeTypes.FLOAT, 2048.0F, AttributeRange.NON_NEGATIVE_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> WATER_FOG_COLOR = of("visual/water_fog_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(-16448205), null, kSync | kSpatial);
    EnvironmentAttribute<Float> WATER_FOG_START_DISTANCE = of("visual/water_fog_start_distance", AttributeTypes.FLOAT, -8.0F, null, kSync | kSpatial);
    EnvironmentAttribute<Float> WATER_FOG_END_DISTANCE = of("visual/water_fog_end_distance", AttributeTypes.FLOAT, 96.0F, AttributeRange.NON_NEGATIVE_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> SKY_COLOR = of("visual/sky_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(0), null, kSync | kSpatial);
    EnvironmentAttribute<Vector4fc> SUNRISE_SUNSET_COLOR = of("visual/sunrise_sunset_color", AttributeTypes.ARGB_COLOR, ARGB.vector4fFromARGB32(0), null, kSync | kSpatial);
    EnvironmentAttribute<Vector4fc> CLOUD_COLOR = of("visual/cloud_color", AttributeTypes.ARGB_COLOR, ARGB.vector4fFromARGB32(0), null, kSync | kSpatial);
    EnvironmentAttribute<Float> CLOUD_HEIGHT = of("visual/cloud_height", AttributeTypes.FLOAT, 192.33F, null, kSync | kSpatial);
    EnvironmentAttribute<Float> SUN_ANGLE = of("visual/sun_angle", AttributeTypes.ANGLE_DEGREES, 0.0F, null, kSync | kSpatial);
    EnvironmentAttribute<Float> MOON_ANGLE = of("visual/moon_angle", AttributeTypes.ANGLE_DEGREES, 0.0F, null, kSync | kSpatial);
    EnvironmentAttribute<Float> STAR_ANGLE = of("visual/star_angle", AttributeTypes.ANGLE_DEGREES, 0.0F, null, kSync | kSpatial);
    EnvironmentAttribute<MoonPhase> MOON_PHASE = of("visual/moon_phase", AttributeTypes.MOON_PHASE, MoonPhase.FULL_MOON, null, kSync);
    EnvironmentAttribute<Float> STAR_BRIGHTNESS = of("visual/star_brightness", AttributeTypes.FLOAT, 0.0F, AttributeRange.UNIT_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> BLOCK_LIGHT_TINT = of("visual/block_light_tint", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(-10100), null, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> SKY_LIGHT_COLOR = of("visual/sky_light_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(-1), null, kSync | kSpatial);
    EnvironmentAttribute<Float> SKY_LIGHT_FACTOR = of("visual/sky_light_factor", AttributeTypes.FLOAT, 1.0F, AttributeRange.UNIT_FLOAT, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> NIGHT_VISION_COLOR = of("visual/night_vision_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(-6710887), null, kSync | kSpatial);
    EnvironmentAttribute<Vector3fc> AMBIENT_LIGHT_COLOR = of("visual/ambient_light_color", AttributeTypes.RGB_COLOR, ARGB.vector3fFromRGB24(-16777216), null, kSync | kSpatial);
    EnvironmentAttribute<ParticleOptions> DEFAULT_DRIPSTONE_PARTICLE = of("visual/default_dripstone_particle", AttributeTypes.PARTICLE, ParticleTypes.DRIPPING_DRIPSTONE_WATER, null, kSync);
    EnvironmentAttribute<List<AmbientParticle>> AMBIENT_PARTICLES = of("visual/ambient_particles", AttributeTypes.AMBIENT_PARTICLES, List.of(), null, kSync);
    EnvironmentAttribute<BackgroundMusic> BACKGROUND_MUSIC = of("audio/background_music", AttributeTypes.BACKGROUND_MUSIC, BackgroundMusic.EMPTY, null, kSync);
    EnvironmentAttribute<Float> MUSIC_VOLUME = of("audio/music_volume", AttributeTypes.FLOAT, 1.0F, AttributeRange.UNIT_FLOAT, kSync);
    EnvironmentAttribute<AmbientSounds> AMBIENT_SOUNDS = of("audio/ambient_sounds", AttributeTypes.AMBIENT_SOUNDS, AmbientSounds.EMPTY, null, kSync);
    EnvironmentAttribute<Boolean> FIREFLY_BUSH_SOUNDS = of("audio/firefly_bush_sounds", AttributeTypes.BOOLEAN, false, null, kSync);
    EnvironmentAttribute<Float> SKY_LIGHT_LEVEL = of("gameplay/sky_light_level", AttributeTypes.FLOAT, 15.0F, AttributeRange.ofFloat(0.0F, 15.0F), kSync | kLocal);
    EnvironmentAttribute<Boolean> CAN_START_RAID = of("gameplay/can_start_raid", AttributeTypes.BOOLEAN, true, null, 0);
    EnvironmentAttribute<Boolean> WATER_EVAPORATES = of("gameplay/water_evaporates", AttributeTypes.BOOLEAN, false, null, kSync);
    EnvironmentAttribute<BedRule> BED_RULE = of("gameplay/bed_rule", AttributeTypes.BED_RULE, BedRule.CAN_SLEEP_WHEN_DARK, null, 0);
    EnvironmentAttribute<BedRule> STRAW_BED_RULE = of("gameplay/straw_bed_rule", AttributeTypes.BED_RULE, BedRule.DESTROY_ON_LEAVE, null, 0);
    EnvironmentAttribute<Boolean> RESPAWN_ANCHOR_WORKS = of("gameplay/respawn_anchor_works", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<Boolean> NETHER_PORTAL_SPAWNS_PIGLINS = of("gameplay/nether_portal_spawns_piglin", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<Boolean> FAST_LAVA = of("gameplay/fast_lava", AttributeTypes.BOOLEAN, false, null, kSync | kLocal);
    EnvironmentAttribute<Boolean> INCREASED_FIRE_BURNOUT = of("gameplay/increased_fire_burnout", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<TriState> EYEBLOSSOM_OPEN = of("gameplay/eyeblossom_open", AttributeTypes.TRI_STATE, TriState.DEFAULT, null, 0);
    EnvironmentAttribute<Float> TURTLE_EGG_HATCH_CHANCE = of("gameplay/turtle_egg_hatch_chance", AttributeTypes.FLOAT, 0.002F, AttributeRange.UNIT_FLOAT, 0);
    EnvironmentAttribute<Boolean> PIGLINS_ZOMBIFY = of("gameplay/piglins_zombify", AttributeTypes.BOOLEAN, true, null, kSync);
    EnvironmentAttribute<Boolean> SNOW_GOLEM_MELTS = of("gameplay/snow_golem_melts", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<Boolean> CREAKING_ACTIVE = of("gameplay/creaking_active", AttributeTypes.BOOLEAN, false, null, kSync);
    EnvironmentAttribute<Float> SURFACE_SLIME_SPAWN_CHANCE = of("gameplay/surface_slime_spawn_chance", AttributeTypes.FLOAT, 0.0F, AttributeRange.UNIT_FLOAT, 0);
    EnvironmentAttribute<Float> CAT_WAKING_UP_GIFT_CHANCE = of("gameplay/cat_waking_up_gift_chance", AttributeTypes.FLOAT, 0.0F, AttributeRange.UNIT_FLOAT, 0);
    EnvironmentAttribute<Boolean> BEES_STAY_IN_HIVE = of("gameplay/bees_stay_in_hive", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<Boolean> MONSTERS_BURN = of("gameplay/monsters_burn", AttributeTypes.BOOLEAN, false, null, 0);
    EnvironmentAttribute<Boolean> CAN_PILLAGER_PATROL_SPAWN = of("gameplay/can_pillager_patrol_spawn", AttributeTypes.BOOLEAN, true, null, 0);
    EnvironmentAttribute<MobSpawnSettings> NATURAL_MOB_SPAWNS = of("gameplay/natural_mob_spawns", AttributeTypes.MOB_SPAWN_SETTINGS, MobSpawnSettings.EMPTY, null, kFullBiomes);
    EnvironmentAttribute<Float> CREATURE_WORLD_GEN_SPAWN_PROBABILITY = of("gameplay/creature_world_gen_spawn_probability", AttributeTypes.FLOAT, 0.1F, AttributeRange.UNIT_FLOAT_EPSILON, 0);
    EnvironmentAttribute<Activity> VILLAGER_ACTIVITY = of("gameplay/villager_activity", AttributeTypes.ACTIVITY, Activity.IDLE, null, 0);
    EnvironmentAttribute<Activity> BABY_VILLAGER_ACTIVITY = of("gameplay/baby_villager_activity", AttributeTypes.ACTIVITY, Activity.IDLE, null, 0);
    Codec<EnvironmentAttribute<?>> CODEC = BuiltInRegistries.ENVIRONMENT_ATTRIBUTE.byNameCodec();

    static EnvironmentAttribute<?> bootstrap(Registry<EnvironmentAttribute<?>> registry) {
        return RESPAWN_ANCHOR_WORKS;
    }

    private static <Value> EnvironmentAttribute<Value> of(String id, AttributeType<Value> type, Value fallback, AttributeRange<Value> range, int flags) {
        EnvironmentAttribute.Builder<Value> builder = EnvironmentAttribute.builder(type).defaultValue(fallback);
        if (range != null) {
            builder.valueRange(range);
        }
        if ((flags & kSync) != 0) {
            builder.syncable();
        }
        if ((flags & kSpatial) != 0) {
            builder.spatiallyInterpolated();
        }
        if ((flags & kLocal) != 0) {
            builder.notPositional();
        }
        if ((flags & kFullBiomes) != 0) {
            builder.fullResolutionBiomes();
        }
        EnvironmentAttribute<Value> attribute = builder.build();
        Registry.register(BuiltInRegistries.ENVIRONMENT_ATTRIBUTE, Identifier.withDefaultNamespace(id), attribute);
        return attribute;
    }
}
