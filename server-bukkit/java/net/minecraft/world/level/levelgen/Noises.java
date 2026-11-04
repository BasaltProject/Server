// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class Noises {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.NOISE,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    public static final ResourceKey<NormalNoise> TEMPERATURE = key("temperature");
    public static final ResourceKey<NormalNoise> VEGETATION = key("vegetation");
    public static final ResourceKey<NormalNoise> CONTINENTALNESS = key("continentalness");
    public static final ResourceKey<NormalNoise> EROSION = key("erosion");
    public static final ResourceKey<NormalNoise> TEMPERATURE_LARGE = key("temperature_large");
    public static final ResourceKey<NormalNoise> VEGETATION_LARGE = key("vegetation_large");
    public static final ResourceKey<NormalNoise> CONTINENTALNESS_LARGE = key("continentalness_large");
    public static final ResourceKey<NormalNoise> EROSION_LARGE = key("erosion_large");
    public static final ResourceKey<NormalNoise> RIDGE = key("ridge");
    public static final ResourceKey<NormalNoise> SHIFT = key("shift");
    public static final ResourceKey<NormalNoise> TEMPERATURE_NETHER = key("temperature_nether");
    public static final ResourceKey<NormalNoise> VEGETATION_NETHER = key("vegetation_nether");
    public static final ResourceKey<NormalNoise> AQUIFER_BARRIER = key("aquifer_barrier");
    public static final ResourceKey<NormalNoise> AQUIFER_FLUID_LEVEL_FLOODEDNESS = key("aquifer_fluid_level_floodedness");
    public static final ResourceKey<NormalNoise> AQUIFER_LAVA = key("aquifer_lava");
    public static final ResourceKey<NormalNoise> AQUIFER_FLUID_LEVEL_SPREAD = key("aquifer_fluid_level_spread");
    public static final ResourceKey<NormalNoise> PILLAR = key("pillar");
    public static final ResourceKey<NormalNoise> PILLAR_RARENESS = key("pillar_rareness");
    public static final ResourceKey<NormalNoise> PILLAR_THICKNESS = key("pillar_thickness");
    public static final ResourceKey<NormalNoise> SPAGHETTI_2D = key("spaghetti_2d");
    public static final ResourceKey<NormalNoise> SPAGHETTI_2D_ELEVATION = key("spaghetti_2d_elevation");
    public static final ResourceKey<NormalNoise> SPAGHETTI_2D_MODULATOR = key("spaghetti_2d_modulator");
    public static final ResourceKey<NormalNoise> SPAGHETTI_2D_THICKNESS = key("spaghetti_2d_thickness");
    public static final ResourceKey<NormalNoise> SPAGHETTI_3D_1 = key("spaghetti_3d_1");
    public static final ResourceKey<NormalNoise> SPAGHETTI_3D_2 = key("spaghetti_3d_2");
    public static final ResourceKey<NormalNoise> SPAGHETTI_3D_RARITY = key("spaghetti_3d_rarity");
    public static final ResourceKey<NormalNoise> SPAGHETTI_3D_THICKNESS = key("spaghetti_3d_thickness");
    public static final ResourceKey<NormalNoise> SPAGHETTI_ROUGHNESS = key("spaghetti_roughness");
    public static final ResourceKey<NormalNoise> SPAGHETTI_ROUGHNESS_MODULATOR = key("spaghetti_roughness_modulator");
    public static final ResourceKey<NormalNoise> CAVE_ENTRANCE = key("cave_entrance");
    public static final ResourceKey<NormalNoise> CAVE_LAYER = key("cave_layer");
    public static final ResourceKey<NormalNoise> CAVE_CHEESE = key("cave_cheese");
    public static final ResourceKey<NormalNoise> ORE_VEININESS = key("ore_veininess");
    public static final ResourceKey<NormalNoise> ORE_VEIN_A = key("ore_vein_a");
    public static final ResourceKey<NormalNoise> ORE_VEIN_B = key("ore_vein_b");
    public static final ResourceKey<NormalNoise> ORE_GAP = key("ore_gap");
    public static final ResourceKey<NormalNoise> NOODLE = key("noodle");
    public static final ResourceKey<NormalNoise> NOODLE_THICKNESS = key("noodle_thickness");
    public static final ResourceKey<NormalNoise> NOODLE_RIDGE_A = key("noodle_ridge_a");
    public static final ResourceKey<NormalNoise> NOODLE_RIDGE_B = key("noodle_ridge_b");
    public static final ResourceKey<NormalNoise> JAGGED = key("jagged");
    public static final ResourceKey<NormalNoise> SURFACE = key("surface");
    public static final ResourceKey<NormalNoise> SURFACE_SECONDARY = key("surface_secondary");
    public static final ResourceKey<NormalNoise> CLAY_BANDS_OFFSET = key("clay_bands_offset");
    public static final ResourceKey<NormalNoise> BADLANDS_PILLAR = key("badlands_pillar");
    public static final ResourceKey<NormalNoise> BADLANDS_PILLAR_ROOF = key("badlands_pillar_roof");
    public static final ResourceKey<NormalNoise> BADLANDS_SURFACE = key("badlands_surface");
    public static final ResourceKey<NormalNoise> ICEBERG_PILLAR = key("iceberg_pillar");
    public static final ResourceKey<NormalNoise> ICEBERG_PILLAR_ROOF = key("iceberg_pillar_roof");
    public static final ResourceKey<NormalNoise> ICEBERG_SURFACE = key("iceberg_surface");
    public static final ResourceKey<NormalNoise> SULFUR_CAVE_GRADIENT = key("sulfur_cave_gradient");
    public static final ResourceKey<NormalNoise> SWAMP = key("swamp");
    public static final ResourceKey<NormalNoise> CALCITE = key("calcite");
    public static final ResourceKey<NormalNoise> GRAVEL = key("gravel");
    public static final ResourceKey<NormalNoise> POWDER_SNOW = key("powder_snow");
    public static final ResourceKey<NormalNoise> PACKED_ICE = key("packed_ice");
    public static final ResourceKey<NormalNoise> ICE = key("ice");
    public static final ResourceKey<NormalNoise> SOUL_SAND_LAYER = key("soul_sand_layer");
    public static final ResourceKey<NormalNoise> GRAVEL_LAYER = key("gravel_layer");
    public static final ResourceKey<NormalNoise> PATCH = key("patch");
    public static final ResourceKey<NormalNoise> SMALL_PATCH = key("small_patch");
    public static final ResourceKey<NormalNoise> NETHERRACK = key("netherrack");
    public static final ResourceKey<NormalNoise> NETHER_WART = key("nether_wart");
    public static final ResourceKey<NormalNoise> NETHER_STATE_SELECTOR = key("nether_state_selector");
    public static Noise instantiate(HolderGetter<NormalNoise> noises, PositionalRandomFactory context, ResourceKey<NormalNoise> name) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.Noises#instantiate");
    }
}
