package ca.spottedleaf.moonrise.patches.chunk_system.level.entity;

import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.entity.LevelEntityGetter;
import org.basaltmc.bukkit.Unsupported;
public abstract class EntityLookup implements LevelEntityGetter {
    protected EntityLookup() {
    }
    public boolean addNewEntity(Entity entity, boolean event) {
        throw Unsupported.hit("ca.spottedleaf.moonrise.patches.chunk_system.level.entity.EntityLookup.addNewEntity");
    }
    public ChunkEntitySlices getChunk(int chunkX, int chunkZ) {
        throw Unsupported.hit("ca.spottedleaf.moonrise.patches.chunk_system.level.entity.EntityLookup.getChunk");
    }
}
