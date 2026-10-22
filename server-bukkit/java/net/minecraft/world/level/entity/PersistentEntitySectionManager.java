package net.minecraft.world.level.entity;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.WorldNatives;

/** The entities of a level, which the server holds: views are made on request and nothing is kept here. */
public class PersistentEntitySectionManager<T> {
    private final ServerLevel level;

    protected PersistentEntitySectionManager() {
        this.level = null;
    }

    public PersistentEntitySectionManager(ServerLevel level) {
        this.level = level;
    }

    public boolean addNewEntity(EntityAccess entity) {
        return ((Entity) entity).spawnIn("custom");
    }

    public List getEntities(ChunkPos chunkPos) {
        String world = level.getWorld().getName();
        List<Entity> found = new ArrayList<>();
        for (int id : WorldNatives.entityIds(world, chunkPos.x(), chunkPos.z(), false)) {
            Entity view = view(id);
            if (view != null) {
                found.add(view);
            }
        }
        return found;
    }

    public LevelEntityGetter<Entity> getEntityGetter() {
        return new LevelEntityGetter<>() {
            @Override
            public EntityAccess get(UUID id) {
                int found = WorldNatives.entityByUuid(level.getWorld().getName(), id.toString());
                return found < 0 ? null : view(found);
            }
        };
    }

    private Entity view(int id) {
        String[] strings = WorldNatives.entityStrings(level.getWorld().getName(), id);
        if (strings == null) {
            return null;
        }
        EntityType<?> type = BuiltInRegistries.ENTITY_TYPE.getValue(Identifier.parse(strings[1]));
        return Entity.view(level, id, UUID.fromString(strings[0]), type);
    }
}
