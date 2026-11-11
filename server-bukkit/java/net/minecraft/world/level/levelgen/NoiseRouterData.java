// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import it.unimi.dsi.fastutil.floats.FloatList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.Vec3i;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.data.worldgen.TerrainProvider;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.Mth;
import net.minecraft.world.level.biome.OverworldBiomeBuilder;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunctions;
import net.minecraft.world.level.levelgen.densityfunction.DistanceMetric;
import net.minecraft.world.level.levelgen.densityfunction.op.SplineFunction;
import net.minecraft.world.level.levelgen.material.rule.OreVeinRule;
import net.minecraft.world.level.levelgen.synth.BlendedNoise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

public class NoiseRouterData {
    public static final float GLOBAL_OFFSET = -0.50375F;
    public static final float NOISE_ZERO = 0.390625F;
    public static final int ISLAND_CHUNK_DISTANCE = 64;
    public static final long ISLAND_CHUNK_DISTANCE_SQR = 4096L;
    private static final int kMinY = -2032;
    private static final int kMaxY = 2031;
    private static final DensityFunction kBlendingFactor = DensityFunctions.constant(10.0F);
    private static final DensityFunction kBlendingJaggedness = DensityFunctions.zero();
    private static final ResourceKey<DensityFunction> kZero = key("zero");
    private static final ResourceKey<DensityFunction> kY = key("y");
    private static final ResourceKey<DensityFunction> kShiftX = key("shift_x");
    private static final ResourceKey<DensityFunction> kShiftZ = key("shift_z");
    private static final ResourceKey<DensityFunction> kBase3dOverworld = key("overworld/base_3d_noise");
    private static final ResourceKey<DensityFunction> kBase3dNether = key("nether/base_3d_noise");
    private static final ResourceKey<DensityFunction> kBase3dEnd = key("end/base_3d_noise");
    public static final ResourceKey<DensityFunction> RIDGES = key("overworld/ridges");
    public static final ResourceKey<DensityFunction> RIDGES_FOLDED = key("overworld/ridges_folded");
    public static final OverworldFunctionSet<ResourceKey<DensityFunction>> OVERWORLD_FUNCTIONS = functionSet("overworld");
    public static final OverworldFunctionSet<ResourceKey<DensityFunction>> AMPLIFIED_OVERWORLD_FUNCTIONS = shiftedSet(
        "overworld_amplified");
    public static final OverworldFunctionSet<ResourceKey<DensityFunction>> LARGE_OVERWORLD_FUNCTIONS = functionSet(
        "overworld_large_biomes");
    private static final ResourceKey<DensityFunction> kEndIslands = key("end/islands");
    private static final ResourceKey<DensityFunction> kSlopedCheeseEnd = key("end/sloped_cheese");
    private static final ResourceKey<DensityFunction> kSpaghettiRoughness = key("overworld/caves/spaghetti_roughness_function");
    private static final ResourceKey<DensityFunction> kEntrances = key("overworld/caves/entrances");
    private static final ResourceKey<DensityFunction> kNoodle = key("overworld/caves/noodle");
    private static final ResourceKey<DensityFunction> kPillars = key("overworld/caves/pillars");
    private static final ResourceKey<DensityFunction> kSpaghetti2dThickness = key("overworld/caves/spaghetti_2d_thickness_modulator");
    private static final ResourceKey<DensityFunction> kSpaghetti2d = key("overworld/caves/spaghetti_2d");
    private static final ResourceKey<DensityFunction> kOreVeinMask = key("overworld/ore_vein/mask");
    private static final ResourceKey<DensityFunction> kOreVeinToggle = key("overworld/ore_vein/toggle");
    public static final ResourceKey<DensityFunction> ORE_VEIN_RICHNESS = key("overworld/ore_vein/richness");
    public static final ResourceKey<DensityFunction> ORE_VEIN_COPPER_DENSITY = key("overworld/ore_vein/copper_density");
    public static final ResourceKey<DensityFunction> ORE_VEIN_IRON_DENSITY = key("overworld/ore_vein/iron_density");
    public static final ResourceKey<DensityFunction> ORE_VEIN_GAP = key("overworld/ore_vein/gap");

    private static ResourceKey<DensityFunction> key(String name) {
        Identifier id = Identifier.withDefaultNamespace(name);
        return ResourceKey.create(Registries.DENSITY_FUNCTION, id);
    }

    private static OverworldFunctionSet<ResourceKey<DensityFunction>> functionSet(String prefix) {
        return new OverworldFunctionSet<>(key(prefix + "/temperature"), key(prefix + "/vegetation"),
            key(prefix + "/continents"), key(prefix + "/erosion"), key(prefix + "/offset"), key(prefix + "/factor"),
            key(prefix + "/jaggedness"), key(prefix + "/depth"), key(prefix + "/sloped_cheese"),
            key(prefix + "/preliminary_surface_level"), key(prefix + "/chunk_surface_level"),
            key(prefix + "/final_density"));
    }

    private static OverworldFunctionSet<ResourceKey<DensityFunction>> shiftedSet(String prefix) {
        OverworldFunctionSet<ResourceKey<DensityFunction>> own = functionSet(prefix);
        OverworldFunctionSet<ResourceKey<DensityFunction>> base = OVERWORLD_FUNCTIONS;
        return new OverworldFunctionSet<>(base.temperature(), base.vegetation(), base.continents(), base.erosion(),
            own.offset(), own.factor(), own.jaggedness(), own.depth(), own.slopedCheese(),
            own.preliminarySurfaceLevel(), own.chunkSurfaceLevel(), own.finalDensity());
    }

    private static DensityFunction register(BootstrapContext<DensityFunction> context, ResourceKey<DensityFunction> name,
        DensityFunction value) {
        Holder<DensityFunction> holder = context.register(name, value);
        return new DensityFunctions.HolderHolder(holder);
    }

    public static void bootstrap(BootstrapContext<DensityFunction> context) {
        HolderGetter<NormalNoise> noises = noiseGetter(context);
        HolderGetter<DensityFunction> functions = functionGetter(context);
        context.register(kZero, DensityFunctions.zero());
        int below = kMinY * 2;
        int above = kMaxY * 2;
        context.register(kY, DensityFunctions.yClampedGradient(below, above, (float) below, (float) above));
        registerOreVeins(context);
        DensityFunction shiftX = register(context, kShiftX, DensityFunctions.cache(DensityFunctions.shiftA(noises.getOrThrow(Noises.SHIFT))));
        DensityFunction shiftZ = register(context, kShiftZ, DensityFunctions.cache(DensityFunctions.shiftB(noises.getOrThrow(Noises.SHIFT))));
        context.register(kBase3dOverworld, new BlendedNoise(0.25, 0.125, 80.0, 160.0, 8.0));
        context.register(kBase3dNether, new BlendedNoise(0.25, 0.375, 80.0, 60.0, 8.0));
        context.register(kBase3dEnd, new BlendedNoise(0.25, 0.25, 80.0, 160.0, 4.0));
        register(context, OVERWORLD_FUNCTIONS.temperature(), shifted(shiftX, shiftZ, noises, Noises.TEMPERATURE));
        register(context, LARGE_OVERWORLD_FUNCTIONS.temperature(), shifted(shiftX, shiftZ, noises, Noises.TEMPERATURE_LARGE));
        register(context, OVERWORLD_FUNCTIONS.vegetation(), shifted(shiftX, shiftZ, noises, Noises.VEGETATION));
        register(context, LARGE_OVERWORLD_FUNCTIONS.vegetation(), shifted(shiftX, shiftZ, noises, Noises.VEGETATION_LARGE));
        DensityFunction continents = register(context, OVERWORLD_FUNCTIONS.continents(),
            DensityFunctions.cache(shifted(shiftX, shiftZ, noises, Noises.CONTINENTALNESS)));
        DensityFunction erosion = register(context, OVERWORLD_FUNCTIONS.erosion(),
            DensityFunctions.cache(shifted(shiftX, shiftZ, noises, Noises.EROSION)));
        DensityFunction ridge = register(context, RIDGES, DensityFunctions.cache(shifted(shiftX, shiftZ, noises, Noises.RIDGE)));
        DensityFunction folded = peaksAndValleys(ridge);
        context.register(RIDGES_FOLDED, folded);
        DensityFunction jagged = DensityFunctions.noise(noises.getOrThrow(Noises.JAGGED), 1500.0, 0.0);
        registerTerrain(context, functions, jagged, continents, erosion, OVERWORLD_FUNCTIONS, false);
        DensityFunction largeContinents = register(context, LARGE_OVERWORLD_FUNCTIONS.continents(),
            DensityFunctions.cache(shifted(shiftX, shiftZ, noises, Noises.CONTINENTALNESS_LARGE)));
        DensityFunction largeErosion = register(context, LARGE_OVERWORLD_FUNCTIONS.erosion(),
            DensityFunctions.cache(shifted(shiftX, shiftZ, noises, Noises.EROSION_LARGE)));
        registerTerrain(context, functions, jagged, largeContinents, largeErosion, LARGE_OVERWORLD_FUNCTIONS, false);
        registerTerrain(context, functions, jagged, continents, erosion, AMPLIFIED_OVERWORLD_FUNCTIONS, true);
        DensityFunction islands = register(context, kEndIslands, endIslands());
        context.register(kSlopedCheeseEnd, DensityFunctions.add(islands, getFunction(functions, kBase3dEnd)));
        context.register(kSpaghettiRoughness, spaghettiRoughness(noises));
        context.register(kSpaghetti2dThickness, DensityFunctions.cache(
            DensityFunctions.mappedNoise(noises.getOrThrow(Noises.SPAGHETTI_2D_THICKNESS), 2.0, 1.0, -0.6F, -1.3F)));
        context.register(kSpaghetti2d, spaghetti2d(functions, noises));
        context.register(kEntrances, entrances(functions, noises));
        context.register(kNoodle, noodle(functions, noises));
        context.register(kPillars, pillars(noises));
    }

    @SuppressWarnings("unchecked")
    private static HolderGetter<NormalNoise> noiseGetter(BootstrapContext<DensityFunction> context) {
        return (HolderGetter<NormalNoise>) (HolderGetter<?>) context.lookup(Registries.NOISE);
    }

    @SuppressWarnings("unchecked")
    private static HolderGetter<DensityFunction> functionGetter(BootstrapContext<DensityFunction> context) {
        return (HolderGetter<DensityFunction>) (HolderGetter<?>) context.lookup(Registries.DENSITY_FUNCTION);
    }

    private static DensityFunction shifted(DensityFunction shiftX, DensityFunction shiftZ, HolderGetter<NormalNoise> noises,
        ResourceKey<NormalNoise> noise) {
        return DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noises.getOrThrow(noise));
    }

    private static DensityFunction endIslands() {
        DensityFunction toCentre = DensityFunctions.distanceToPoint(Vec3i.ZERO, DistanceMetric.EUCLIDEAN);
        DensityFunction main = DensityFunctions.sliceY(
            DensityFunctions.constant(100.0F).sub(toCentre).clamp(-100.0F, 80.0F).sub(8.0F).mul(0.0078125F), 0);
        return DensityFunctions.cache(DensityFunctions.max(main, DensityFunctions.endOuterIslands()));
    }

    private static void registerTerrain(BootstrapContext<DensityFunction> context, HolderGetter<DensityFunction> functions,
        DensityFunction jaggedNoise, DensityFunction continentsFunction, DensityFunction erosionFunction,
        OverworldFunctionSet<ResourceKey<DensityFunction>> names, boolean amplified) {
        var continents = new SplineFunction.Coordinate(continentsFunction);
        var erosion = new SplineFunction.Coordinate(erosionFunction);
        var weirdness = new SplineFunction.Coordinate(getFunction(functions, RIDGES));
        var ridges = new SplineFunction.Coordinate(getFunction(functions, RIDGES_FOLDED));
        DensityFunction offset = register(context, names.offset(), blendedSpline(DensityFunctions.add(
            DensityFunctions.constant(GLOBAL_OFFSET),
            DensityFunctions.spline(TerrainProvider.overworldOffset(continents, erosion, ridges, amplified))),
            DensityFunctions.blendOffset()));
        DensityFunction factor = register(context, names.factor(), blendedSpline(DensityFunctions.spline(
            TerrainProvider.overworldFactor(continents, erosion, weirdness, ridges, amplified)), kBlendingFactor));
        DensityFunction depth = register(context, names.depth(), offsetToDepth(offset));
        DensityFunction rawJaggedness = register(context, names.jaggedness(), blendedSpline(DensityFunctions.spline(
            TerrainProvider.overworldJaggedness(continents, erosion, weirdness, ridges, amplified)), kBlendingJaggedness));
        DensityFunction jaggedness = DensityFunctions.cache(DensityFunctions.mul(rawJaggedness, jaggedNoise.halfNegative()));
        DensityFunction initial = noiseGradient(factor, DensityFunctions.add(depth, jaggedness));
        DensityFunction slopedCheese = register(context, names.slopedCheese(),
            DensityFunctions.cache(DensityFunctions.add(initial, getFunction(functions, kBase3dOverworld))));
        DensityFunction surface = register(context, names.preliminarySurfaceLevel(),
            preliminarySurface(offset, factor, amplified));
        context.register(names.chunkSurfaceLevel(), DensityFunctions.interpolated(surface, 16, 1));
        DensityFunction withEntrances = DensityFunctions.min(slopedCheese, getFunction(functions, kEntrances).mul(5.0F));
        DensityFunction caves = DensityFunctions.rangeChoice(slopedCheese, -1000000.0F, 1.5625F, withEntrances,
            underground(functions, noiseOf(context), slopedCheese));
        context.register(names.finalDensity(), DensityFunctions.add(DensityFunctions.min(
            postProcess(slideOverworld(amplified, caves), 4, 8), getFunction(functions, kNoodle)),
            DensityFunctions.beardifier()));
    }

    private static HolderGetter<NormalNoise> noiseOf(BootstrapContext<DensityFunction> context) {
        return noiseGetter(context);
    }

    private static DensityFunction offsetToDepth(DensityFunction offset) {
        DensityFunction gradient = DensityFunctions.yClampedGradient(-64, 320, 1.5F, -1.5F);
        return DensityFunctions.add(gradient, offset);
    }

    public static DensityFunction getFunction(HolderGetter<DensityFunction> functions, ResourceKey<DensityFunction> name) {
        Holder<DensityFunction> holder = functions.getOrThrow(name);
        return new DensityFunctions.HolderHolder(holder);
    }

    public static DensityFunction peaksAndValleys(DensityFunction weirdness) {
        DensityFunction folded = DensityFunctions.add(weirdness.abs(), DensityFunctions.constant(-0.6666667F)).abs();
        return DensityFunctions.mul(DensityFunctions.add(folded, DensityFunctions.constant(-0.33333334F)),
            DensityFunctions.constant(-3.0F));
    }

    public static float peaksAndValleys(float weirdness) {
        float value = TerrainProvider.peaksAndValleys(weirdness);
        return value;
    }

    private static DensityFunction spaghettiRoughness(HolderGetter<NormalNoise> noises) {
        DensityFunction base = DensityFunctions.noise(noises.getOrThrow(Noises.SPAGHETTI_ROUGHNESS));
        DensityFunction modulator = DensityFunctions.mappedNoise(noises.getOrThrow(Noises.SPAGHETTI_ROUGHNESS_MODULATOR), 0.0F, -0.1F);
        return DensityFunctions.cache(DensityFunctions.mul(modulator,
            DensityFunctions.add(base.abs(), DensityFunctions.constant(-0.4F))));
    }

    private static DensityFunction entrances(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises) {
        DensityFunction rarity = DensityFunctions.cache(DensityFunctions.noise(noises.getOrThrow(Noises.SPAGHETTI_3D_RARITY), 2.0, 1.0));
        DensityFunction thickness = DensityFunctions.mappedNoise(noises.getOrThrow(Noises.SPAGHETTI_3D_THICKNESS), -0.065F, -0.088F);
        DensityFunction cave1 = QuantizedSpaghettiRarity.wrapRarity3d(rarity, noises.getOrThrow(Noises.SPAGHETTI_3D_1));
        DensityFunction cave2 = QuantizedSpaghettiRarity.wrapRarity3d(rarity, noises.getOrThrow(Noises.SPAGHETTI_3D_2));
        DensityFunction spaghetti3d = DensityFunctions.add(DensityFunctions.max(cave1, cave2), thickness).clamp(-1.0F, 1.0F);
        DensityFunction roughness = getFunction(functions, kSpaghettiRoughness);
        DensityFunction bigSource = DensityFunctions.noise(noises.getOrThrow(Noises.CAVE_ENTRANCE), 0.75, 0.5);
        DensityFunction big = DensityFunctions.add(bigSource.add(0.37F), DensityFunctions.yClampedGradient(-10, 30, 0.3F, 0.0F));
        return DensityFunctions.cache(DensityFunctions.min(big, DensityFunctions.add(roughness, spaghetti3d)));
    }

    private static DensityFunction noodle(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises) {
        DensityFunction y = getFunction(functions, kY);
        DensityFunction toggle = limited(y, DensityFunctions.noise(noises.getOrThrow(Noises.NOODLE), 1.0, 1.0), -60, 320, -1, 4, 8);
        DensityFunction thickness = limited(y, DensityFunctions.mappedNoise(noises.getOrThrow(Noises.NOODLE_THICKNESS), 1.0, 1.0, -0.05F, -0.1F), -60, 320, 0, 4, 8);
        double frequency = 2.6666666666666665;
        DensityFunction ridgeA = limited(y, DensityFunctions.noise(noises.getOrThrow(Noises.NOODLE_RIDGE_A), frequency, frequency), -60, 320, 0, 4, 8);
        DensityFunction ridgeB = limited(y, DensityFunctions.noise(noises.getOrThrow(Noises.NOODLE_RIDGE_B), frequency, frequency), -60, 320, 0, 4, 8);
        DensityFunction ridged = DensityFunctions.max(ridgeA.abs(), ridgeB.abs()).mul(1.5F);
        return DensityFunctions.rangeChoice(toggle, -1000000.0F, 0.0F, DensityFunctions.constant(64.0F),
            DensityFunctions.add(thickness, ridged));
    }

    private static DensityFunction pillars(HolderGetter<NormalNoise> noises) {
        DensityFunction source = DensityFunctions.noise(noises.getOrThrow(Noises.PILLAR), 25.0, 0.3);
        DensityFunction rareness = DensityFunctions.mappedNoise(noises.getOrThrow(Noises.PILLAR_RARENESS), 0.0F, -2.0F);
        DensityFunction thickness = DensityFunctions.mappedNoise(noises.getOrThrow(Noises.PILLAR_THICKNESS), 0.0F, 1.1F);
        return DensityFunctions.cache(DensityFunctions.mul(DensityFunctions.add(source.mul(2.0F), rareness), thickness.cube()));
    }

    private static DensityFunction spaghetti2d(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises) {
        DensityFunction modulator = DensityFunctions.noise(noises.getOrThrow(Noises.SPAGHETTI_2D_MODULATOR), 2.0, 1.0);
        DensityFunction cave = QuantizedSpaghettiRarity.wrapRarity2d(modulator, noises.getOrThrow(Noises.SPAGHETTI_2D));
        DensityFunction elevation = DensityFunctions.mappedNoise(noises.getOrThrow(Noises.SPAGHETTI_2D_ELEVATION), 0.0,
            (float) Math.floorDiv(-64, 8), 8.0F);
        DensityFunction thickness = getFunction(functions, kSpaghetti2dThickness);
        DensityFunction sloped = DensityFunctions.add(DensityFunctions.cache(elevation),
            DensityFunctions.yClampedGradient(-64, 320, 8.0F, -40.0F)).abs();
        DensityFunction ridged = DensityFunctions.add(sloped, thickness).cube();
        DensityFunction caveNoise = DensityFunctions.add(cave, thickness.mul(0.083F));
        return DensityFunctions.max(caveNoise, ridged).clamp(-1.0F, 1.0F);
    }

    private static DensityFunction underground(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises,
        DensityFunction slopedCheese) {
        DensityFunction layers = DensityFunctions.noise(noises.getOrThrow(Noises.CAVE_LAYER), 8.0).square().mul(4.0F);
        Holder<NormalNoise> cheeseNoise = noises.getOrThrow(Noises.CAVE_CHEESE);
        DensityFunction cheese = DensityFunctions.noise(cheeseNoise, 2.0 / 3.0);
        DensityFunction solid = DensityFunctions.add(cheese.add(0.27F).clamp(-1.0F, 1.0F),
            slopedCheese.mul(-0.64F).add(1.5F).clamp(0.0F, 0.5F));
        DensityFunction base = DensityFunctions.add(layers, solid);
        DensityFunction subtractions = DensityFunctions.min(DensityFunctions.min(base, getFunction(functions, kEntrances)),
            DensityFunctions.add(getFunction(functions, kSpaghetti2d), getFunction(functions, kSpaghettiRoughness)));
        DensityFunction raw = getFunction(functions, kPillars);
        DensityFunction cut = DensityFunctions.rangeChoice(raw, -1000000.0F, 0.03F, DensityFunctions.constant(-1000000.0F), raw);
        return DensityFunctions.max(subtractions, cut);
    }

    private static DensityFunction postProcess(DensityFunction slide, int cellSizeXz, int cellSizeY) {
        DensityFunction blended = DensityFunctions.blendDensity(slide);
        return DensityFunctions.interpolated(DensityFunctions.mul(blended, DensityFunctions.constant(0.64F)), cellSizeXz,
            cellSizeY).squeeze();
    }

    protected static NoiseRouter overworld(HolderGetter<DensityFunction> functions,
        OverworldFunctionSet<ResourceKey<DensityFunction>> functionNames) {
        OverworldFunctionSet<DensityFunction> set = functionNames.map(name -> getFunction(functions, name));
        return new NoiseRouter(set.temperature(), set.vegetation(), set.continents(), set.erosion(), set.depth(),
            getFunction(functions, RIDGES), set.chunkSurfaceLevel(), set.finalDensity());
    }

    protected static Aquifer.Config overworldAquifers(HolderGetter<DensityFunction> functions,
        HolderGetter<NormalNoise> noises, OverworldFunctionSet<ResourceKey<DensityFunction>> names) {
        DensityFunction barrier = DensityFunctions.noise(noises.getOrThrow(Noises.AQUIFER_BARRIER), 0.5);
        DensityFunction floodedness = DensityFunctions.noise(noises.getOrThrow(Noises.AQUIFER_FLUID_LEVEL_FLOODEDNESS), 0.67);
        DensityFunction spread = DensityFunctions.noise(noises.getOrThrow(Noises.AQUIFER_FLUID_LEVEL_SPREAD), 0.7142857142857143);
        DensityFunction lava = DensityFunctions.noise(noises.getOrThrow(Noises.AQUIFER_LAVA));
        DensityFunction exclusion = OverworldBiomeBuilder.deepDarkRegion(getFunction(functions, names.erosion()),
            getFunction(functions, names.depth()));
        return new Aquifer.Config(barrier, floodedness, spread, lava, exclusion,
            getFunction(functions, names.preliminarySurfaceLevel()));
    }

    private static void registerOreVeins(BootstrapContext<DensityFunction> context) {
        HolderGetter<DensityFunction> functions = functionGetter(context);
        HolderGetter<NormalNoise> noises = noiseGetter(context);
        DensityFunction y = getFunction(functions, kY);
        int veinLow = Math.min(OreVeinRule.VeinType.COPPER.minY, OreVeinRule.VeinType.IRON.minY);
        int veinHigh = Math.max(OreVeinRule.VeinType.COPPER.maxY, OreVeinRule.VeinType.IRON.maxY);
        int low = Mth.floorDiv(veinLow, 8) * 8;
        int high = (Mth.floorDiv(veinHigh, 8) + 1) * 8;
        DensityFunction toggle = register(context, kOreVeinToggle, DensityFunctions.cache(limited(y,
            DensityFunctions.noise(noises.getOrThrow(Noises.ORE_VEININESS), 1.5, 1.5), low, high, 0, 4, 8)));
        context.register(ORE_VEIN_RICHNESS, DensityFunctions.clampedMap(toggle.abs(), 0.4F, 0.6F, 0.1F, 0.3F));
        DensityFunction mask = register(context, kOreVeinMask, DensityFunctions.cache(oreVeinMask(noises, y, toggle, low, high)));
        context.register(ORE_VEIN_COPPER_DENSITY, oreVeinDensity(OreVeinRule.VeinType.COPPER, y, toggle, mask, true));
        context.register(ORE_VEIN_IRON_DENSITY, oreVeinDensity(OreVeinRule.VeinType.IRON, y, toggle, mask, false));
        DensityFunction gap = DensityFunctions.noise(noises.getOrThrow(Noises.ORE_GAP));
        context.register(ORE_VEIN_GAP, DensityFunctions.constant(-0.3F).sub(gap));
    }

    private static DensityFunction oreVeinMask(HolderGetter<NormalNoise> noises, DensityFunction y, DensityFunction toggle,
        int minY, int maxY) {
        DensityFunction veinA = limited(y, DensityFunctions.noise(noises.getOrThrow(Noises.ORE_VEIN_A), 4.0, 4.0), minY, maxY, 1, 4, 8).abs();
        DensityFunction veinB = limited(y, DensityFunctions.noise(noises.getOrThrow(Noises.ORE_VEIN_B), 4.0, 4.0), minY, maxY, 1, 4, 8).abs();
        return DensityFunctions.rangeChoice(toggle, -0.4F, 0.4F, DensityFunctions.constant(-1.0F),
            DensityFunctions.constant(0.08F).sub(DensityFunctions.max(veinA, veinB)));
    }

    private static DensityFunction oreVeinDensity(OreVeinRule.VeinType type, DensityFunction y, DensityFunction toggle,
        DensityFunction baseMask, boolean whenTogglePositive) {
        DensityFunction none = DensityFunctions.constant(-1.0F);
        DensityFunction edge = DensityFunctions.min(DensityFunctions.constant((float) type.maxY).sub(y), y.sub((float) type.minY));
        DensityFunction roundoff = DensityFunctions.clampedMap(edge, 0.0F, 20.0F, -0.2F, 0.0F);
        DensityFunction veininess = !whenTogglePositive ? toggle.negate() : toggle;
        DensityFunction inner = DensityFunctions.rangeChoice(veininess.sub(0.4F).add(roundoff), 0.0F, 1000000.0F,
            DensityFunctions.constant(0.7F), none);
        DensityFunction masked = DensityFunctions.rangeChoice(baseMask, 0.0F, 1000000.0F, inner, none);
        return DensityFunctions.rangeChoice(y, (float) type.minY, (float) type.maxY, masked, none);
    }

    private static DensityFunction slideOverworld(boolean amplified, DensityFunction caves) {
        return slide(caves, -64, 384, amplified ? 16 : 80, amplified ? 0 : 64, -0.078125F, 0, 24,
            amplified ? 0.4F : 0.1171875F);
    }

    private static DensityFunction slideNetherLike(HolderGetter<DensityFunction> functions, int minY, int height) {
        return slide(getFunction(functions, kBase3dNether), minY, height, 24, 0, 0.9375F, -8, 24, 2.5F);
    }

    private static DensityFunction slideEndLike(DensityFunction caves, int minY, int height) {
        return slide(caves, minY, height, 72,
            -184, -23.4375F, 4, 32, -0.234375F);
    }

    protected static NoiseRouter nether(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises) {
        DensityFunction zero = DensityFunctions.zero();
        DensityFunction temperature = DensityFunctions.shiftedNoise2d(zero, zero, 0.25, noises.getOrThrow(Noises.TEMPERATURE_NETHER));
        DensityFunction vegetation = DensityFunctions.shiftedNoise2d(zero, zero, 0.25, noises.getOrThrow(Noises.VEGETATION_NETHER));
        DensityFunction full = DensityFunctions.add(postProcess(slideNetherLike(functions, 0, 128), 4, 8),
            DensityFunctions.beardifier());
        return new NoiseRouter(temperature, vegetation, zero, zero, zero, zero, zero, full);
    }

    protected static NoiseRouter caves(HolderGetter<DensityFunction> functions) {
        return simpleRouter(DensityFunctions.add(postProcess(slideNetherLike(functions, -64, 192), 4, 8),
            DensityFunctions.beardifier()));
    }

    protected static NoiseRouter floatingIslands(HolderGetter<DensityFunction> functions, HolderGetter<NormalNoise> noises) {
        DensityFunction slide = slideEndLike(getFunction(functions, kBase3dEnd), 0, 256);
        DensityFunction post = postProcess(slide, 8, 4);
        return simpleRouter(DensityFunctions.add(post, DensityFunctions.beardifier()));
    }

    protected static NoiseRouter end(HolderGetter<DensityFunction> functions) {
        DensityFunction zero = DensityFunctions.zero();
        DensityFunction slide = slideEndLike(getFunction(functions, kSlopedCheeseEnd), 0, 128);
        DensityFunction full = DensityFunctions.add(postProcess(slide, 8, 4), DensityFunctions.beardifier());
        return new NoiseRouter(zero, zero, zero, getFunction(functions, kEndIslands), zero, zero, zero, full);
    }

    private static NoiseRouter simpleRouter(DensityFunction fullNoise) {
        DensityFunction zero = DensityFunctions.zero();
        return new NoiseRouter(zero, zero, zero, zero, zero, zero, zero, fullNoise);
    }

    public static NoiseRouter none() {
        DensityFunction zero = DensityFunctions.zero();
        return simpleRouter(zero);
    }

    private static DensityFunction blendedSpline(DensityFunction spline, DensityFunction blendingTarget) {
        DensityFunction alpha = DensityFunctions.blendAlpha();
        return DensityFunctions.cache(DensityFunctions.lerp(alpha, blendingTarget, spline));
    }

    private static DensityFunction noiseGradient(DensityFunction factor, DensityFunction depthWithJaggedness) {
        return DensityFunctions.mul(depthWithJaggedness, factor).quarterNegative().mul(4.0F);
    }

    private static DensityFunction preliminarySurface(DensityFunction offset, DensityFunction factor, boolean amplified) {
        DensityFunction bound = DensityFunctions.remap(
            DensityFunctions.sub(DensityFunctions.div(DensityFunctions.constant(0.2734375F), factor), offset), 1.5F, -1.5F,
            -64.0F, 320.0F).clamp(-40.0F, 320.0F);
        DensityFunction gradient = DensityFunctions.add(noiseGradient(factor, offsetToDepth(offset)),
            DensityFunctions.constant(-0.703125F)).clamp(-64.0F, 64.0F);
        DensityFunction density = DensityFunctions.add(slideOverworld(amplified, gradient), DensityFunctions.constant(-0.390625F));
        return DensityFunctions.findTopSurface(density, bound, -64, 8);
    }

    private static DensityFunction limited(DensityFunction y, DensityFunction inRange, int minInclusive, int maxInclusive,
        int outOfRange, int cellSizeXz, int cellSizeY) {
        return DensityFunctions.interpolated(DensityFunctions.rangeChoice(y, (float) minInclusive,
            (float) (maxInclusive + 1), inRange, DensityFunctions.constant((float) outOfRange)), cellSizeXz, cellSizeY);
    }

    private static DensityFunction slide(DensityFunction caves, int minY, int height, int topStartY, int topEndY,
        float topTarget, int bottomStartY, int bottomEndY, float bottomTarget) {
        int top = minY + height;
        DensityFunction topFactor = DensityFunctions.yClampedGradient(top - topStartY, top - topEndY, 1.0F, 0.0F);
        DensityFunction value = DensityFunctions.lerp(topFactor, topTarget, caves);
        DensityFunction bottomFactor = DensityFunctions.yClampedGradient(bottomStartY + minY, bottomEndY + minY, 0.0F, 1.0F);
        return DensityFunctions.lerp(bottomFactor, bottomTarget, value);
    }

    protected static final class QuantizedSpaghettiRarity {
        public static DensityFunction wrapRarity2d(DensityFunction input, Holder<NormalNoise> noise) {
            return DensityFunctions.intervalSelect(input, FloatList.of(-0.75F, -0.5F, 0.5F, 0.75F),
                List.of(rarity(noise, 0.5F), rarity(noise, 0.75F), rarity(noise, 1.0F), rarity(noise, 2.0F),
                    rarity(noise, 3.0F))).abs();
        }

        public static DensityFunction wrapRarity3d(DensityFunction input, Holder<NormalNoise> noise) {
            return DensityFunctions.intervalSelect(input, FloatList.of(-0.5F, 0.0F, 0.5F),
                List.of(rarity(noise, 0.75F), rarity(noise, 1.0F), rarity(noise, 1.5F), rarity(noise, 2.0F))).abs();
        }

        private static DensityFunction rarity(Holder<NormalNoise> noise, float rarity) {
            return DensityFunctions.noise(noise, 1.0 / rarity, 1.0 / rarity).mul(rarity);
        }
    }
}
