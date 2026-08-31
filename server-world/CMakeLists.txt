basalt_module(server-world
    SOURCES
        src/Chunk.cpp
        src/ChunkDataCodec.cpp
        src/ChunkSection.cpp
        src/ChunkTypes.cpp
        src/PalettedContainer.cpp
        src/climate/Climate.cpp
        src/climate/OverworldBiomes.cpp
        src/densityfunction/DensityCache.cpp
        src/densityfunction/DensityFunction.cpp
        src/light/Light.cpp
        src/light/LightSpread.cpp
        src/light/LightWindow.cpp
        src/noise/ImprovedNoise.cpp
        src/region/ChunkNbt.cpp
        src/structure/StructureBounds.cpp
        src/terrain/carver/NetherCaveCarver.cpp
    src/terrain/carver/OverworldCarvers.cpp
    src/terrain/decoration/AquaticPatchDecoration.cpp
    src/terrain/decoration/BiomeDecoration.cpp
    src/terrain/decoration/BiomeFlowerFeatures.cpp
    src/terrain/decoration/FireflyBushPatchDecoration.cpp
    src/terrain/decoration/FlowerFeatures.cpp
    src/terrain/decoration/FlowerPatchDecoration.cpp
    src/terrain/decoration/FlowerVegetation.cpp
    src/terrain/decoration/ForestRockDecoration.cpp
    src/terrain/decoration/GrassScatterPatches.cpp
    src/terrain/decoration/GlowLichenDecoration.cpp
    src/terrain/decoration/GrassPatchDecoration.cpp
    src/terrain/decoration/GroundPlantPatchDecoration.cpp
    src/terrain/decoration/GroundVegetation.cpp
    src/terrain/decoration/IceSpikesDecoration.cpp
    src/terrain/decoration/IntProvider.cpp
    src/terrain/decoration/OreBlockRules.cpp
    src/terrain/decoration/OverworldOreConfigs.cpp
    src/terrain/decoration/OverworldSpringDecoration.cpp
    src/terrain/decoration/OverworldOreDecoration.cpp
    src/terrain/decoration/PlacementRegion.cpp
    src/terrain/decoration/BushPatchDecoration.cpp
    src/terrain/decoration/CactusFeature.cpp
    src/terrain/decoration/MelonPatchDecoration.cpp
    src/terrain/decoration/CaveVineColumnFeature.cpp
    src/terrain/decoration/DesertWellDecoration.cpp
    src/terrain/decoration/DoublePlantPatchDecoration.cpp
    src/terrain/decoration/MushroomPatchDecoration.cpp
    src/terrain/decoration/PatchRegion.cpp
    src/terrain/decoration/PlainsFlowerFeatures.cpp
    src/terrain/decoration/TallGrassDecoration.cpp
    src/terrain/decoration/VinePatchDecoration.cpp
    src/terrain/decoration/SugarCanePatchDecoration.cpp
    src/terrain/decoration/SurfaceFreezingDecoration.cpp
    src/terrain/decoration/SurfacePatches.cpp
    src/terrain/decoration/Placement.cpp
    src/terrain/decoration/SimpleBlockFeature.cpp
    src/terrain/BiomeClimateData.cpp
    src/terrain/surface/BadlandsClayBands.cpp
    src/terrain/surface/SurfaceBuilder.cpp
    src/terrain/surface/ChunkBiomeEnvironment.cpp
    src/terrain/surface/SurfaceRuleContext.cpp
    src/terrain/surface/SurfaceRuleData.cpp
    src/terrain/surface/SurfaceRuleEnvironment.cpp
    src/terrain/surface/SurfaceRules.cpp
    src/terrain/BiomeTemperature.cpp
    src/terrain/BiomeZoomer.cpp
    src/terrain/TerrainProvider.cpp
    src/terrain/NoiseRouterData.cpp
    src/terrain/OverworldAquifer.cpp
        src/noise/NormalNoise.cpp
        src/noise/PerlinNoise.cpp
        src/noise/SimplexNoise.cpp
    src/noise/PerlinSimplexNoise.cpp
    DEPENDS basalt::server-random basalt::server-registry basalt::wire-common basalt::wire-protocol
)

basalt_test(server-world-noise-test
    SOURCES
        test/NoiseTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-light-test
    SOURCES
        test/LightTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-cave-vine-test
    SOURCES
        test/CaveVineTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-glow-lichen-test
    SOURCES
        test/GlowLichenTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-spring-test
    SOURCES
        test/SpringTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ice-spikes-test
    SOURCES
        test/IceSpikesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-desert-well-test
    SOURCES
        test/DesertWellTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-forest-rock-test
    SOURCES
        test/ForestRockTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-surface-freezing-test
    SOURCES
        test/SurfaceFreezingTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-flower-patch-test
    SOURCES
        test/FlowerPatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-plains-flower-test
    SOURCES
        test/PlainsFlowerTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-biome-flower-test
    SOURCES
        test/BiomeFlowerTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-flower-features-test
    SOURCES
        test/FlowerFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tall-grass-test
    SOURCES
        test/TallGrassTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-grass-patch-test
    SOURCES
        test/GrassPatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-aquatic-test
    SOURCES
        test/AquaticTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-firefly-bush-test
    SOURCES
        test/FireflyBushTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ground-plant-test
    SOURCES
        test/GroundPlantTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-bush-test
    SOURCES
        test/BushTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-vine-test
    SOURCES
        test/VineTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-mushroom-patch-test
    SOURCES
        test/MushroomPatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-double-plant-test
    SOURCES
        test/DoublePlantTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-melon-test
    SOURCES
        test/MelonTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-sugar-cane-test
    SOURCES
        test/SugarCaneTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-grass-scatter-test
    SOURCES
        test/GrassScatterTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-cactus-test
    SOURCES
        test/CactusTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-surface-patch-test
    SOURCES
        test/SurfacePatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-flower-vegetation-test
    SOURCES
        test/FlowerVegetationTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ground-vegetation-test
    SOURCES
        test/GroundVegetationTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-simple-block-test
    SOURCES
        test/SimpleBlockTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ore-test
    SOURCES
        test/OreTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-placement-test
    SOURCES
        test/PlacementTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-carver-test
    SOURCES
        test/CarverTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-surface-test
    SOURCES
        test/SurfaceTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-biome-test
    SOURCES
        test/BiomeTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-aquifer-test
    SOURCES
        test/AquiferTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-noise-router-test
    SOURCES
        test/NoiseRouterTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-terrain-test
    SOURCES
        test/TerrainTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-climate-test
    SOURCES
        test/ClimateTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-density-test
    SOURCES
        test/DensityTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-spline-test
    SOURCES
        test/SplineTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-region-test
    SOURCES
        test/RegionTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-chunk-test
    SOURCES
        test/ChunkTest.cpp
    DEPENDS basalt::server-world
)

basalt_bench(server-world-noise-bench
    SOURCES
        bench/NoiseBench.cpp
    DEPENDS basalt::server-world
)
