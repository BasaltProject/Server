package net.minecraft.world.level.chunk;

import net.minecraft.core.Holder;
import org.basaltmc.bukkit.Unsupported;
public class LevelChunkSection {
    public LevelChunkSection(PalettedContainer palettedContainer, PalettedContainer palettedContainer2) {
    }
    public LevelChunkSection(PalettedContainerFactory palettedContainerFactory) {
    }
    public PalettedContainerRO getBiomes() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunkSection.getBiomes");
    }
    public Holder getNoiseBiome(int quartX, int quartY, int quartZ) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunkSection.getNoiseBiome");
    }
    public PalettedContainer getStates() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunkSection.getStates");
    }
    public boolean hasOnlyAir() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunkSection.hasOnlyAir");
    }
}
