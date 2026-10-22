package net.minecraft.world.level.entity;

import java.util.UUID;
import org.basaltmc.bukkit.Unsupported;
public interface LevelEntityGetter<T> {
    public default EntityAccess get(UUID id) {
        throw Unsupported.hit("net.minecraft.world.level.entity.LevelEntityGetter.get");
    }
}
