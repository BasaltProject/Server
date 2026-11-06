// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import com.google.common.collect.ImmutableList;
import java.util.ArrayList;
import java.util.List;
import java.util.function.UnaryOperator;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;

public record ChunkPyramid(ImmutableList<ChunkStep> steps) {
    public static final ChunkPyramid GENERATION_PYRAMID = new Builder()
        .step(ChunkStatus.EMPTY, s -> s)
        .step(ChunkStatus.STRUCTURE_STARTS, s -> s.setTask(ChunkStatusTasks::generateStructureStarts))
        .step(ChunkStatus.STRUCTURE_REFERENCES, s -> s.addRequirement(ChunkStatus.STRUCTURE_STARTS, 8).setTask(ChunkStatusTasks::generateStructureReferences))
        .step(ChunkStatus.BIOMES, s -> s.addRequirement(ChunkStatus.STRUCTURE_STARTS, 8).setTask(ChunkStatusTasks::generateBiomes))
        .step(ChunkStatus.TERRAIN, s -> s.addRequirement(ChunkStatus.STRUCTURE_STARTS, 8).addRequirement(ChunkStatus.BIOMES, 1).blockStateWriteRadius(0).setTask(ChunkStatusTasks::buildTerrain))
        .step(ChunkStatus.FEATURES, s -> s.addRequirement(ChunkStatus.STRUCTURE_STARTS, 8).addRequirement(ChunkStatus.TERRAIN, 1).blockStateWriteRadius(1).setTask(ChunkStatusTasks::generateFeatures))
        .step(ChunkStatus.INITIALIZE_LIGHT, s -> s.setTask(ChunkStatusTasks::initializeLight))
        .step(ChunkStatus.LIGHT, s -> s.addRequirement(ChunkStatus.INITIALIZE_LIGHT, 1).setTask(ChunkStatusTasks::light))
        .step(ChunkStatus.SPAWN, s -> s.addRequirement(ChunkStatus.BIOMES, 1).setTask(ChunkStatusTasks::generateSpawn))
        .step(ChunkStatus.FULL, s -> s.setTask(ChunkStatusTasks::full))
        .build();
    public static final ChunkPyramid LOADING_PYRAMID = new Builder()
        .step(ChunkStatus.EMPTY, s -> s)
        .step(ChunkStatus.STRUCTURE_STARTS, s -> s.setTask(ChunkStatusTasks::loadStructureStarts))
        .step(ChunkStatus.STRUCTURE_REFERENCES, s -> s)
        .step(ChunkStatus.BIOMES, s -> s)
        .step(ChunkStatus.TERRAIN, s -> s)
        .step(ChunkStatus.FEATURES, s -> s)
        .step(ChunkStatus.INITIALIZE_LIGHT, s -> s.setTask(ChunkStatusTasks::initializeLight))
        .step(ChunkStatus.LIGHT, s -> s.addRequirement(ChunkStatus.INITIALIZE_LIGHT, 1).setTask(ChunkStatusTasks::light))
        .step(ChunkStatus.SPAWN, s -> s)
        .step(ChunkStatus.FULL, s -> s.setTask(ChunkStatusTasks::full))
        .build();
    private static final int kSafetyMarginChunks = (32 + GENERATION_PYRAMID.getStepTo(ChunkStatus.FULL).accumulatedDependencies().size() + 1) * 2;
    public static final int MAX_CHUNK_COORDINATE_VALUE = SectionPos.blockToSectionCoord(BlockPos.MAX_HORIZONTAL_COORDINATE) - kSafetyMarginChunks;

    public ChunkStep getStepTo(ChunkStatus status) {
        return steps.get(status.getIndex());
    }

    public static class Builder {
        private final List<ChunkStep> steps = new ArrayList<>();

        public ChunkPyramid build() {
            return new ChunkPyramid(ImmutableList.copyOf(steps));
        }

        public Builder step(ChunkStatus status, UnaryOperator<ChunkStep.Builder> operator) {
            ChunkStep.Builder builder = steps.isEmpty() ? new ChunkStep.Builder(status) : new ChunkStep.Builder(status, steps.getLast());
            steps.add(operator.apply(builder).build());
            return this;
        }
    }
}
