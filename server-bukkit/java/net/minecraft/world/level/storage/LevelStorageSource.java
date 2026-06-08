package net.minecraft.world.level.storage;

import java.nio.file.Path;
import net.minecraft.resources.ResourceKey;
import org.basaltmc.bukkit.Unsupported;
public class LevelStorageSource {
    protected LevelStorageSource() {
    }
    public LevelStorageSource.LevelStorageAccess createAccess(String levelId) {
        throw Unsupported.hit("net.minecraft.world.level.storage.LevelStorageSource.createAccess");
    }
    public static LevelStorageSource createDefault(Path path) {
        throw Unsupported.hit("net.minecraft.world.level.storage.LevelStorageSource.createDefault");
    }

    public static class LevelStorageAccess {
        protected LevelStorageAccess() {
        }
        public void close() {
            throw Unsupported.hit("net.minecraft.world.level.storage.LevelStorageSource.LevelStorageAccess.close");
        }
        public Path getDimensionPath(ResourceKey name) {
            throw Unsupported.hit("net.minecraft.world.level.storage.LevelStorageSource.LevelStorageAccess.getDimensionPath");
        }
    }
}
