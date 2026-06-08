package net.minecraft.world.level;

import net.minecraft.core.RegistryAccess;
import org.basaltmc.bukkit.Unsupported;
public interface LevelAccessor extends CommonLevelAccessor, ScheduledTickAccess {
    public default RegistryAccess registryAccess() {
        throw Unsupported.hit("net.minecraft.world.level.LevelAccessor.registryAccess");
    }
}
