package net.minecraft.world.level.entity;

import java.util.List;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.Unsupported;
public class PersistentEntitySectionManager<T> {
    protected PersistentEntitySectionManager() {
    }
    public boolean addNewEntity(EntityAccess entity) {
        throw Unsupported.hit("net.minecraft.world.level.entity.PersistentEntitySectionManager.addNewEntity");
    }
    public List getEntities(ChunkPos chunkPos) {
        throw Unsupported.hit("net.minecraft.world.level.entity.PersistentEntitySectionManager.getEntities");
    }
}
