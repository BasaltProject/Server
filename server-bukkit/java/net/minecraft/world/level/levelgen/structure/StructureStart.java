package net.minecraft.world.level.levelgen.structure;

import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import org.basaltmc.bukkit.Unsupported;
public class StructureStart {
    protected StructureStart() {
    }
    public BoundingBox getBoundingBox() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.structure.StructureStart.getBoundingBox");
    }
    public boolean isValid() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.structure.StructureStart.isValid");
    }
    public void placeInChunk(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.structure.StructureStart.placeInChunk");
    }
}
