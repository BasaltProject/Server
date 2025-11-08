basalt_module(server-world
    SOURCES
        src/Chunk.cpp
        src/ChunkTracker.cpp
        src/GenerationMetrics.cpp
        src/LeafDistances.cpp
        src/portal/PortalShape.cpp
        src/ChunkDataCodec.cpp
        src/ChunkSection.cpp
        src/ChunkTypes.cpp
        src/PalettedContainer.cpp
        src/climate/Climate.cpp
    src/climate/SpawnFinder.cpp
        src/climate/OverworldBiomes.cpp
        src/densityfunction/DensityCache.cpp
        src/densityfunction/DensityFunction.cpp
        src/light/Light.cpp
        src/light/LightSpread.cpp
        src/light/LightWindow.cpp
        src/noise/ImprovedNoise.cpp
        src/region/ChunkNbt.cpp
        src/structure/StrongholdBrush.cpp
    src/structure/StrongholdCorridors.cpp
    src/structure/StrongholdLayout.cpp
    src/structure/StrongholdLibrary.cpp
    src/structure/StrongholdPortalRoom.cpp
    src/structure/StrongholdRooms.cpp
    src/structure/StructureBounds.cpp
    src/structure/ConcentricRingsPlacement.cpp
    src/structure/JigsawAssembly.cpp
    src/structure/MansionExterior.cpp
    src/structure/MansionGrid.cpp
    src/structure/MansionPlacer.cpp
    src/structure/MansionRoomCollection.cpp
    src/structure/MineshaftBlocks.cpp
    src/structure/MineshaftFill.cpp
    src/structure/MonumentBrush.cpp
    src/structure/MonumentDoubleRooms.cpp
    src/structure/MonumentRoomFitters.cpp
    src/structure/MonumentRoomGraph.cpp
    src/structure/MonumentShell.cpp
    src/structure/MonumentSingleRooms.cpp
    src/structure/MonumentWings.cpp
    src/structure/MineshaftLayout.cpp
    src/structure/StructurePieceFill.cpp
    src/structure/StructurePlacementGrid.cpp
    src/structure/StructureProcessor.cpp
    src/structure/StructureQueries.cpp
    src/structure/StructureSpawnOverrides.cpp
    src/structure/StructureTemplate.cpp
    src/structure/StructureTemplatePool.cpp
    src/structure/StructureTemplateTransform.cpp
    src/structure/TemplateBlockProcessors.cpp
    src/structure/StructureTransform.cpp
    src/structure/VillagePoolJson.cpp
        src/terrain/carver/NetherCaveCarver.cpp
    src/terrain/carver/OverworldCarvers.cpp
    src/terrain/decoration/AquaticDecoration.cpp
    src/terrain/decoration/AquaticPatchDecoration.cpp
    src/terrain/decoration/AquaticSupport.cpp
    src/terrain/decoration/BiomeDecoration.cpp
    src/terrain/decoration/BiomeFlowerFeatures.cpp
    src/terrain/decoration/ExtraVegetationDecoration.cpp
    src/terrain/decoration/FireflyBushPatchDecoration.cpp
    src/terrain/decoration/FlowerFeatures.cpp
    src/terrain/decoration/FossilFeature.cpp
    src/terrain/decoration/FlowerPatchDecoration.cpp
    src/terrain/decoration/FlowerVegetation.cpp
    src/terrain/decoration/ForestRockDecoration.cpp
    src/terrain/decoration/GrassScatterPatches.cpp
    src/terrain/decoration/GlowLichenDecoration.cpp
    src/terrain/decoration/GrassPatchDecoration.cpp
    src/terrain/decoration/GroundPlantPatchDecoration.cpp
    src/terrain/decoration/GroundVegetation.cpp
    src/terrain/decoration/IceSpikesDecoration.cpp
    src/terrain/decoration/HugeMushroomFeature.cpp
    src/terrain/decoration/IntProvider.cpp
    src/terrain/decoration/OreBlockRules.cpp
    src/terrain/decoration/end/EndDecoration.cpp
    src/terrain/decoration/end/EndSurfaceStructuresDecoration.cpp
    src/terrain/decoration/nether/NetherForestVegetation.cpp
    src/terrain/decoration/nether/NetherHugeFungus.cpp
    src/terrain/decoration/nether/NetherLavaSpring.cpp
    src/terrain/decoration/nether/NetherLocalDecoration.cpp
    src/terrain/decoration/nether/NetherMushroom.cpp
    src/terrain/decoration/nether/NetherOreDecoration.cpp
    src/terrain/decoration/nether/NetherSurfaceStructuresDecoration.cpp
    src/terrain/decoration/nether/NetherUndergroundDecoration.cpp
    src/terrain/decoration/nether/NetherVegetalDecoration.cpp
    src/terrain/decoration/nether/NetherVegetalShared.cpp
    src/terrain/decoration/nether/NetherVines.cpp
    src/terrain/decoration/OverworldLakeDecoration.cpp
    src/terrain/decoration/OverworldOreConfigs.cpp
    src/terrain/decoration/LushCaveVegetationDecoration.cpp
    src/terrain/decoration/MushroomFieldsDecoration.cpp
    src/terrain/decoration/OverworldBiomePaths.cpp
    src/terrain/decoration/OverworldSpringDecoration.cpp
    src/terrain/decoration/OverworldUndergroundStructures.cpp
    src/terrain/decoration/OverworldVegetalDecoration.cpp
    src/terrain/decoration/OverworldOreDecoration.cpp
    src/terrain/decoration/PlacedFeatures.cpp
    src/terrain/decoration/PlacedFeaturesContextual.cpp
    src/terrain/decoration/PlacementRegion.cpp
    src/terrain/decoration/BushPatchDecoration.cpp
    src/terrain/decoration/BambooDecoration.cpp
    src/terrain/decoration/BambooVegetationDecoration.cpp
    src/terrain/decoration/CactusFeature.cpp
    src/terrain/decoration/CaveLocalModificationDecoration.cpp
    src/terrain/decoration/CoralDecoration.cpp
    src/terrain/decoration/FrozenOceanDecoration.cpp
    src/terrain/decoration/IcebergFeature.cpp
    src/terrain/decoration/CaveSpeleothemDecoration.cpp
    src/terrain/decoration/SculkDecoration.cpp
    src/terrain/decoration/SculkVeinSpread.cpp
    src/terrain/decoration/JungleFeatures.cpp
    src/terrain/decoration/LushVegetationPatchFeature.cpp
    src/terrain/decoration/MelonPatchDecoration.cpp
    src/terrain/decoration/CaveVineColumnFeature.cpp
    src/terrain/decoration/DesertWellDecoration.cpp
    src/terrain/decoration/DoublePlantPatchDecoration.cpp
    src/terrain/decoration/MushroomPatchDecoration.cpp
    src/terrain/decoration/PaleMossDecoration.cpp
    src/terrain/decoration/PatchRegion.cpp
    src/terrain/decoration/PlainsFlowerFeatures.cpp
    src/terrain/decoration/TaigaGroundDecoration.cpp
    src/terrain/decoration/TallGrassDecoration.cpp
    src/terrain/decoration/TreeDecoration.cpp
    src/terrain/decoration/TreePatchDecoration.cpp
    src/terrain/decoration/tree/FallenTreeFeature.cpp
    src/terrain/decoration/tree/MangroveRootPlacer.cpp
    src/terrain/decoration/tree/TreeBlockRules.cpp
    src/terrain/decoration/tree/TreeBranchingTrunkPlacers.cpp
    src/terrain/decoration/tree/TreeConfigs.cpp
    src/terrain/decoration/tree/TreeFeature.cpp
    src/terrain/decoration/tree/TreeFeatureDecorators.cpp
    src/terrain/decoration/tree/TreeFoliagePlacers.cpp
    src/terrain/decoration/tree/TreeHelpers.cpp
    src/terrain/decoration/tree/TreeTrunkPlacers.cpp
    src/terrain/decoration/VillageFeatures.cpp
    src/terrain/decoration/VinePatchDecoration.cpp
    src/terrain/decoration/SmallFeatures.cpp
    src/terrain/decoration/StandaloneFeatures.cpp
    src/terrain/decoration/SugarCanePatchDecoration.cpp
    src/terrain/decoration/SurfaceFreezingDecoration.cpp
    src/terrain/decoration/SurfacePatches.cpp
    src/terrain/decoration/Placement.cpp
    src/terrain/decoration/RootedAzaleaFeature.cpp
    src/terrain/decoration/RootedSulfurSpringFeature.cpp
    src/terrain/decoration/SimpleBlockFeature.cpp
    src/terrain/BiomeClimateData.cpp
    src/terrain/BubbleColumnFormation.cpp
    src/terrain/DensityChunkSampler.cpp
    src/terrain/NetherBiomeSource.cpp
    src/terrain/TheEndBiomeSource.cpp
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
    src/terrain/WoodlandMansionStructureGenerator.cpp
    src/terrain/MineshaftStructureGenerator.cpp
    src/terrain/NeighbourTerrain.cpp
    src/terrain/NoiseColumnHeights.cpp
    src/terrain/StrongholdStructureGenerator.cpp
    src/terrain/StructureBeardifier.cpp
    src/terrain/OverworldBiomeSource.cpp
    src/terrain/NoiseRouterData.cpp
    src/terrain/OverworldOreVeinifier.cpp
    src/terrain/OceanMonumentStructureGenerator.cpp
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

basalt_test(server-world-tree-types-test
    SOURCES
        test/TreeTypesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-block-rules-test
    SOURCES
        test/TreeBlockRulesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-trunk-placers-test
    SOURCES
        test/TreeTrunkPlacersTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-foliage-placers-test
    SOURCES
        test/TreeFoliagePlacersTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-branching-trunk-placers-test
    SOURCES
        test/TreeBranchingTrunkPlacersTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-feature-test
    SOURCES
        test/TreeFeatureTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-fallen-tree-test
    SOURCES
        test/FallenTreeTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-selector-test
    SOURCES
        test/TreeSelectorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-tree-patch-test
    SOURCES
        test/TreePatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-biome-flora-test
    SOURCES
        test/BiomeFloraTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-extra-vegetation-test
    SOURCES
        test/ExtraVegetationTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-lush-vegetation-test
    SOURCES
        test/LushVegetationTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-standalone-features-test
    SOURCES
        test/StandaloneFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-small-patch-test
    SOURCES
        test/SmallPatchTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-jungle-features-test
    SOURCES
        test/JungleFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-cave-local-modification-test
    SOURCES
        test/CaveLocalModificationTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-sculk-test
    SOURCES
        test/SculkTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-frozen-ocean-test
    SOURCES
        test/FrozenOceanTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-taiga-mushroom-test
    SOURCES
        test/TaigaMushroomTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-lush-cave-test
    SOURCES
        test/LushCaveTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-bamboo-test
    SOURCES
        test/BambooTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-aquatic-vegetal-test
    SOURCES
        test/AquaticVegetalTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-small-features-test
    SOURCES
        test/SmallFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-village-features-test
    SOURCES
        test/VillageFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-overworld-vegetal-test
    SOURCES
        test/OverworldVegetalTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-jigsaw-assembly-test
    SOURCES
        test/JigsawAssemblyTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-mineshaft-generator-test
    SOURCES
        test/MineshaftGeneratorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-mineshaft-fill-test
    SOURCES
        test/MineshaftFillTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-mineshaft-layout-test
    SOURCES
        test/MineshaftLayoutTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-stronghold-generator-test
    SOURCES
        test/StrongholdGeneratorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-stronghold-brush-test
    SOURCES
        test/StrongholdBrushTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-stronghold-pieces-test
    SOURCES
        test/StrongholdPiecesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-stronghold-layout-test
    SOURCES
        test/StrongholdLayoutTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-woodland-mansion-generator-test
    SOURCES
        test/WoodlandMansionGeneratorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-mansion-test
    SOURCES
        test/MansionTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ocean-monument-generator-test
    SOURCES
        test/OceanMonumentGeneratorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-monument-pieces-test
    SOURCES
        test/MonumentPiecesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-monument-rooms-test
    SOURCES
        test/MonumentRoomsTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-pool-and-piece-test
    SOURCES
        test/PoolAndPieceTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-template-block-processors-test
    SOURCES
        test/TemplateBlockProcessorsTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-lake-sulfur-spring-test
    SOURCES
        test/LakeAndSulfurSpringTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-placed-features-test
    SOURCES
        test/PlacedFeaturesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-ore-vein-column-test
    SOURCES
        test/OreVeinColumnTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-overworld-biome-source-test
    SOURCES
        test/OverworldBiomeSourceTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-leaf-distances-test
    SOURCES
        test/LeafDistancesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-chunk-tracker-test
    SOURCES
        test/ChunkTrackerTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-generation-metrics-test
    SOURCES
        test/GenerationMetricsTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-neighbour-terrain-test
    SOURCES
        test/NeighbourTerrainTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-beardifier-bubble-test
    SOURCES
        test/BeardifierBubbleTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-spawn-finder-test
    SOURCES
        test/SpawnFinderTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-portal-shape-test
    SOURCES
        test/PortalShapeTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-structure-transform-test
    SOURCES
        test/StructureTransformTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-structure-placement-test
    SOURCES
        test/StructurePlacementTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-structure-tables-test
    SOURCES
        test/StructureTablesTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-structure-processor-test
    SOURCES
        test/StructureProcessorTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-structure-template-test
    SOURCES
        test/StructureTemplateTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-underground-structures-test
    SOURCES
        test/UndergroundStructuresTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-coral-test
    SOURCES
        test/CoralTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-cave-speleothem-test
    SOURCES
        test/CaveSpeleothemTest.cpp
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

basalt_test(server-world-pale-moss-test
    SOURCES
        test/PaleMossTest.cpp
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

basalt_test(server-world-nether-biome-source-test
    SOURCES
        test/NetherBiomeSourceTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-nether-vegetal-test
    SOURCES
        test/NetherVegetalTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-nether-local-test
    SOURCES
        test/NetherLocalTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-nether-ore-test
    SOURCES
        test/NetherOreTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-nether-surface-structures-test
    SOURCES
        test/NetherSurfaceStructuresTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-nether-underground-test
    SOURCES
        test/NetherUndergroundTest.cpp
    DEPENDS basalt::server-world
)

basalt_test(server-world-end-decoration-test
    SOURCES
        test/EndDecorationTest.cpp
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
